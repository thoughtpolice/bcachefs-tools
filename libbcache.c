#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <uuid/uuid.h>

#include "ccan/ilog/ilog.h"

#include "bcache-ondisk.h"
#include "libbcache.h"
#include "crypto.h"

#define BCH_MIN_NR_NBUCKETS	(1 << 10)

/* first bucket should start 1 mb in, in sectors: */
#define FIRST_BUCKET_OFFSET	(1 << 11)

void __do_write_sb(int fd, void *sb, size_t bytes)
{
	char zeroes[SB_SECTOR << 9] = {0};

	/* Zero start of disk */
	if (pwrite(fd, zeroes, SB_SECTOR << 9, 0) != SB_SECTOR << 9) {
		perror("write error trying to zero start of disk\n");
		exit(EXIT_FAILURE);
	}
	/* Write superblock */
	if (pwrite(fd, sb, bytes, SB_SECTOR << 9) != bytes) {
		perror("write error trying to write superblock\n");
		exit(EXIT_FAILURE);
	}

	fsync(fd);
	close(fd);
}

#define do_write_sb(_fd, _sb)			\
	__do_write_sb(_fd, _sb, ((void *) __bset_bkey_last(_sb)) - (void *) _sb);

void bcache_format(struct dev_opts *devs, size_t nr_devs,
		   unsigned block_size,
		   unsigned btree_node_size,
		   unsigned meta_csum_type,
		   unsigned data_csum_type,
		   unsigned compression_type,
		   const char *passphrase,
		   unsigned meta_replicas,
		   unsigned data_replicas,
		   unsigned on_error_action,
		   char *label,
		   uuid_le uuid)
{
	struct cache_sb *sb;
	struct dev_opts *i;

	/* calculate block size: */
	if (!block_size)
		for (i = devs; i < devs + nr_devs; i++)
			block_size = max(block_size,
					 get_blocksize(i->path, i->fd));

	/* calculate bucket sizes: */
	for (i = devs; i < devs + nr_devs; i++) {
		if (!i->size)
			i->size = get_size(i->path, i->fd);

		if (!i->bucket_size) {
			/*
			 * minimum size filesystem we can create, given a bucket
			 * size:
			 */
			u64 min_size(unsigned bucket_size) {
				return (DIV_ROUND_UP(FIRST_BUCKET_OFFSET, bucket_size) +
					BCH_MIN_NR_NBUCKETS) * bucket_size;
			}

			if (i->size < min_size(block_size))
				die("cannot format %s, too small (%llu sectors, min %llu)",
				    i->path, i->size, min_size(block_size));

			/* Want a bucket size of at least 128k, if possible: */
			i->bucket_size = max(block_size, 256U);

			if (i->size >= min_size(i->bucket_size)) {
				unsigned scale = max(1U,
					ilog2(i->size / min_size(i->bucket_size)) / 4);

				/* max bucket size 1 mb */
				i->bucket_size = min(i->bucket_size * scale, 1U << 11);
			} else {
				do {
					i->bucket_size /= 2;
				} while (i->size < min_size(i->bucket_size));
			}
		}

		/* first bucket: 1 mb in */
		i->first_bucket	= DIV_ROUND_UP(FIRST_BUCKET_OFFSET, i->bucket_size);
		i->nbuckets	= i->size / i->bucket_size;

		if (i->bucket_size < block_size)
			die("Bucket size cannot be smaller than block size");

		if (i->nbuckets - i->first_bucket < BCH_MIN_NR_NBUCKETS)
			die("Not enough buckets: %llu, need %u (bucket size %u)",
			    i->nbuckets - i->first_bucket, BCH_MIN_NR_NBUCKETS,
			    i->bucket_size);
	}

	/* calculate btree node size: */
	if (!btree_node_size) {
		/* 256k default btree node size */
		btree_node_size = 512;

		for (i = devs; i < devs + nr_devs; i++)
			btree_node_size = min(btree_node_size, i->bucket_size);
	}

	sb = calloc(1, sizeof(*sb) + sizeof(struct cache_member) * nr_devs);

	sb->offset	= __cpu_to_le64(SB_SECTOR);
	sb->version	= __cpu_to_le64(BCACHE_SB_VERSION_CDEV_V3);
	sb->magic	= BCACHE_MAGIC;
	sb->block_size	= __cpu_to_le16(block_size);
	sb->user_uuid	= uuid;
	sb->nr_in_set	= nr_devs;

	uuid_generate(sb->set_uuid.b);

	if (label)
		strncpy((char *) sb->label, label, sizeof(sb->label));

	/*
	 * don't have a userspace crc32c implementation handy, just always use
	 * crc64
	 */
	SET_CACHE_SB_CSUM_TYPE(sb,		BCH_CSUM_CRC64);
	SET_CACHE_SET_META_CSUM_TYPE(sb,	meta_csum_type);
	SET_CACHE_SET_DATA_CSUM_TYPE(sb,	data_csum_type);
	SET_CACHE_SET_COMPRESSION_TYPE(sb,	compression_type);

	SET_CACHE_SET_BTREE_NODE_SIZE(sb,	btree_node_size);
	SET_CACHE_SET_META_REPLICAS_WANT(sb,	meta_replicas);
	SET_CACHE_SET_META_REPLICAS_HAVE(sb,	meta_replicas);
	SET_CACHE_SET_DATA_REPLICAS_WANT(sb,	data_replicas);
	SET_CACHE_SET_DATA_REPLICAS_HAVE(sb,	data_replicas);
	SET_CACHE_SET_ERROR_ACTION(sb,		on_error_action);
	SET_CACHE_SET_STR_HASH_TYPE(sb,		BCH_STR_HASH_SIPHASH);

	if (passphrase) {
		struct bcache_key key;
		struct bcache_disk_key disk_key;

		derive_passphrase(&key, passphrase);
		disk_key_init(&disk_key);
		disk_key_encrypt(sb, &disk_key, &key);

		memcpy(sb->encryption_key, &disk_key, sizeof(disk_key));
		SET_CACHE_SET_ENCRYPTION_TYPE(sb, 1);
		SET_CACHE_SET_ENCRYPTION_KEY(sb, 1);

		memzero_explicit(&disk_key, sizeof(disk_key));
		memzero_explicit(&key, sizeof(key));
	}

	for (i = devs; i < devs + nr_devs; i++) {
		struct cache_member *m = sb->members + (i - devs);

		uuid_generate(m->uuid.b);
		m->nbuckets	= __cpu_to_le64(i->nbuckets);
		m->first_bucket	= __cpu_to_le16(i->first_bucket);
		m->bucket_size	= __cpu_to_le16(i->bucket_size);

		SET_CACHE_TIER(m,		i->tier);
		SET_CACHE_REPLACEMENT(m,	CACHE_REPLACEMENT_LRU);
		SET_CACHE_DISCARD(m,		i->discard);
	}

	sb->u64s = __cpu_to_le16(bch_journal_buckets_offset(sb));

	for (i = devs; i < devs + nr_devs; i++) {
		struct cache_member *m = sb->members + (i - devs);
		char uuid_str[40], set_uuid_str[40];

		sb->disk_uuid	= m->uuid;
		sb->nr_this_dev	= i - devs;
		sb->csum	= __cpu_to_le64(__csum_set(sb, __le16_to_cpu(sb->u64s),
							   CACHE_SB_CSUM_TYPE(sb)));

		uuid_unparse(sb->disk_uuid.b, uuid_str);
		uuid_unparse(sb->user_uuid.b, set_uuid_str);
		printf("UUID:			%s\n"
		       "Set UUID:		%s\n"
		       "version:		%u\n"
		       "nbuckets:		%llu\n"
		       "block_size:		%u\n"
		       "bucket_size:		%u\n"
		       "nr_in_set:		%u\n"
		       "nr_this_dev:		%u\n"
		       "first_bucket:		%u\n",
		       uuid_str, set_uuid_str,
		       (unsigned) sb->version,
		       __le64_to_cpu(m->nbuckets),
		       __le16_to_cpu(sb->block_size),
		       __le16_to_cpu(m->bucket_size),
		       sb->nr_in_set,
		       sb->nr_this_dev,
		       __le16_to_cpu(m->first_bucket));

		do_write_sb(i->fd, sb);
	}

	free(sb);
}

void bcache_super_read(const char *path, struct cache_sb *sb)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		die("couldn't open %s", path);

	if (pread(fd, sb, sizeof(*sb), SB_SECTOR << 9) != sizeof(*sb))
		die("error reading superblock");

	if (memcmp(&sb->magic, &BCACHE_MAGIC, sizeof(sb->magic)))
		die("not a bcache superblock");
}
