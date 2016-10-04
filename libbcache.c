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

const char * const cache_state[] = {
	"active",
	"ro",
	"failed",
	"spare",
	NULL
};

const char * const replacement_policies[] = {
	"lru",
	"fifo",
	"random",
	NULL
};

const char * const csum_types[] = {
	"none",
	"crc32c",
	"crc64",
	NULL
};

const char * const compression_types[] = {
	"none",
	"lz4",
	"gzip",
	NULL
};

const char * const str_hash_types[] = {
	"crc32c",
	"crc64",
	"siphash",
	"sha1",
	NULL
};

const char * const error_actions[] = {
	"continue",
	"readonly",
	"panic",
	NULL
};

const char * const member_states[] = {
	"active",
	"ro",
	"failed",
	"spare",
	NULL
};

const char * const bdev_cache_mode[] = {
	"writethrough",
	"writeback",
	"writearound",
	"none",
	NULL
};

const char * const bdev_state[] = {
	"detached",
	"clean",
	"dirty",
	"inconsistent",
	NULL
};

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

/* minimum size filesystem we can create, given a bucket size: */
static u64 min_size(unsigned bucket_size)
{
	return (DIV_ROUND_UP(FIRST_BUCKET_OFFSET, bucket_size) +
		BCH_MIN_NR_NBUCKETS) * bucket_size;
}

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
			if (i->size < min_size(block_size))
				die("cannot format %s, too small (%llu sectors, min %llu)",
				    i->path, i->size, min_size(block_size));

			/* Want a bucket size of at least 128k, if possible: */
			i->bucket_size = max(block_size, 256U);

			if (i->size >= min_size(i->bucket_size)) {
				unsigned scale = max(1U,
					ilog2(i->size / min_size(i->bucket_size)) / 4);

				scale = rounddown_pow_of_two(scale);

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

		sb->disk_uuid	= m->uuid;
		sb->nr_this_dev	= i - devs;
		sb->csum	= __cpu_to_le64(__csum_set(sb, __le16_to_cpu(sb->u64s),
							   CACHE_SB_CSUM_TYPE(sb)));

		do_write_sb(i->fd, sb);
	}

	bcache_super_print(sb, HUMAN_READABLE);

	free(sb);
}

void bcache_super_print(struct cache_sb *sb, int units)
{
	unsigned i;
	char user_uuid_str[40], internal_uuid_str[40], member_uuid_str[40];
	char label[SB_LABEL_SIZE + 1];

	memset(label, 0, sizeof(label));
	memcpy(label, sb->label, sizeof(sb->label));
	uuid_unparse(sb->user_uuid.b, user_uuid_str);
	uuid_unparse(sb->set_uuid.b, internal_uuid_str);

	printf("External UUID:			%s\n"
	       "Internal UUID:			%s\n"
	       "Label:				%s\n"
	       "Version:			%llu\n"
	       "Block_size:			%s\n"
	       "Btree node size:		%s\n"
	       "Error action:			%s\n"
	       "Clean:				%llu\n"

	       "Metadata replicas:		have %llu, want %llu\n"
	       "Data replicas:			have %llu, want %llu\n"

	       "Metadata checksum type:		%s\n"
	       "Data checksum type:		%s\n"
	       "Compression type:		%s\n"

	       "String hash type:		%s\n"
	       "32 bit inodes:			%llu\n"
	       "GC reserve percentage:		%llu%%\n"
	       "Root reserve percentage:	%llu%%\n"

	       "Devices:			%u\n",
	       user_uuid_str,
	       internal_uuid_str,
	       label,
	       le64_to_cpu(sb->version),
	       pr_units(le16_to_cpu(sb->block_size), units).b,
	       pr_units(CACHE_SET_BTREE_NODE_SIZE(sb), units).b,

	       CACHE_SET_ERROR_ACTION(sb) < BCH_NR_ERROR_ACTIONS
	       ? error_actions[CACHE_SET_ERROR_ACTION(sb)]
	       : "unknown",

	       CACHE_SET_CLEAN(sb),

	       CACHE_SET_META_REPLICAS_HAVE(sb),
	       CACHE_SET_META_REPLICAS_WANT(sb),
	       CACHE_SET_DATA_REPLICAS_HAVE(sb),
	       CACHE_SET_DATA_REPLICAS_WANT(sb),

	       CACHE_SET_META_CSUM_TYPE(sb) < BCH_CSUM_NR
	       ? csum_types[CACHE_SET_META_CSUM_TYPE(sb)]
	       : "unknown",

	       CACHE_SET_DATA_CSUM_TYPE(sb) < BCH_CSUM_NR
	       ? csum_types[CACHE_SET_DATA_CSUM_TYPE(sb)]
	       : "unknown",

	       CACHE_SET_COMPRESSION_TYPE(sb) < BCH_COMPRESSION_NR
	       ? compression_types[CACHE_SET_COMPRESSION_TYPE(sb)]
	       : "unknown",

	       CACHE_SET_STR_HASH_TYPE(sb) < BCH_STR_HASH_NR
	       ? str_hash_types[CACHE_SET_STR_HASH_TYPE(sb)]
	       : "unknown",

	       CACHE_INODE_32BIT(sb),
	       CACHE_SET_GC_RESERVE(sb),
	       CACHE_SET_ROOT_RESERVE(sb),

	       sb->nr_in_set);

	for (i = 0; i < sb->nr_in_set; i++) {
		struct cache_member *m = sb->members + i;
		time_t last_mount = le64_to_cpu(m->last_mount);

		uuid_unparse(m->uuid.b, member_uuid_str);

		printf("\n"
		       "Device %u:\n"
		       "  UUID:				%s\n"
		       "  bucket_size:			%s\n"
		       "  first_bucket:			%u\n"
		       "  nbuckets:			%llu\n"
		       "  Last mount:			%s\n"
		       "  State:			%s\n"
		       "  Tier:				%llu\n"
		       "  Has metadata:			%llu\n"
		       "  Has data:			%llu\n"
		       "  Replacement policy:		%s\n"
		       "  Discard:			%llu\n",
		       i, member_uuid_str,
		       pr_units(le16_to_cpu(m->bucket_size), units).b,
		       le16_to_cpu(m->first_bucket),
		       le64_to_cpu(m->nbuckets),
		       last_mount ? ctime(&last_mount) : "(never)",

		       CACHE_STATE(m) < CACHE_STATE_NR
		       ? member_states[CACHE_STATE(m)]
		       : "unknown",

		       CACHE_TIER(m),
		       CACHE_HAS_METADATA(m),
		       CACHE_HAS_DATA(m),

		       CACHE_REPLACEMENT(m) < CACHE_REPLACEMENT_NR
		       ? replacement_policies[CACHE_REPLACEMENT(m)]
		       : "unknown",

		       CACHE_DISCARD(m));
	}
}

struct cache_sb *bcache_super_read(const char *path)
{
	struct cache_sb sb, *ret;
	size_t bytes;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		die("couldn't open %s", path);

	if (pread(fd, &sb, sizeof(sb), SB_SECTOR << 9) != sizeof(sb))
		die("error reading superblock");

	if (memcmp(&sb.magic, &BCACHE_MAGIC, sizeof(sb.magic)))
		die("not a bcache superblock");

	bytes = sizeof(sb) + le16_to_cpu(sb.u64s) * sizeof(u64);

	ret = calloc(1, bytes);

	if (pread(fd, ret, bytes, SB_SECTOR << 9) != bytes)
		die("error reading superblock");

	return ret;
}
