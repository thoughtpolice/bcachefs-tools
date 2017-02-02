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

#include "linux/bcache.h"
#include "libbcache.h"
#include "checksum.h"
#include "opts.h"

#define BCH_MIN_NR_NBUCKETS	(1 << 10)

/* first bucket should start 1 mb in, in sectors: */
#define FIRST_BUCKET_OFFSET	(1 << 11)

void __do_write_sb(int fd, void *sb, size_t bytes)
{
	char zeroes[SB_SECTOR << 9] = {0};

	/* Zero start of disk */
	xpwrite(fd, zeroes, SB_SECTOR << 9, 0);

	/* Write superblock */
	xpwrite(fd, sb, bytes, SB_SECTOR << 9);

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
		   unsigned meta_replicas,
		   unsigned data_replicas,
		   unsigned on_error_action,
		   unsigned max_journal_entry_size,
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
			i->size = get_size(i->path, i->fd) >> 9;

		if (!i->bucket_size) {
			if (i->size < min_size(block_size))
				die("cannot format %s, too small (%llu sectors, min %llu)",
				    i->path, i->size, min_size(block_size));

			/* Want a bucket size of at least 128k, if possible: */
			i->bucket_size = max(block_size, 256U);

			if (i->size >= min_size(i->bucket_size)) {
				unsigned scale = max(1,
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

	if (!max_journal_entry_size) {
		/* 2 MB default: */
		max_journal_entry_size = 4096;
	}

	max_journal_entry_size = roundup_pow_of_two(max_journal_entry_size);

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
	SET_CACHE_SET_META_PREFERRED_CSUM_TYPE(sb,	meta_csum_type);
	SET_CACHE_SET_DATA_PREFERRED_CSUM_TYPE(sb,	data_csum_type);
	SET_CACHE_SET_COMPRESSION_TYPE(sb,	compression_type);

	SET_CACHE_SET_BTREE_NODE_SIZE(sb,	btree_node_size);
	SET_CACHE_SET_META_REPLICAS_WANT(sb,	meta_replicas);
	SET_CACHE_SET_META_REPLICAS_HAVE(sb,	meta_replicas);
	SET_CACHE_SET_DATA_REPLICAS_WANT(sb,	data_replicas);
	SET_CACHE_SET_DATA_REPLICAS_HAVE(sb,	data_replicas);
	SET_CACHE_SET_ERROR_ACTION(sb,		on_error_action);
	SET_CACHE_SET_STR_HASH_TYPE(sb,		BCH_STR_HASH_SIPHASH);
	SET_CACHE_SET_JOURNAL_ENTRY_SIZE(sb,	ilog2(max_journal_entry_size));

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
	       "Max journal entry size:		%s\n"
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
	       pr_units(le16_to_cpu(sb->block_size), units),
	       pr_units(CACHE_SET_BTREE_NODE_SIZE(sb), units),
	       pr_units(1U << CACHE_SET_JOURNAL_ENTRY_SIZE(sb), units),

	       CACHE_SET_ERROR_ACTION(sb) < BCH_NR_ERROR_ACTIONS
	       ? bch_error_actions[CACHE_SET_ERROR_ACTION(sb)]
	       : "unknown",

	       CACHE_SET_CLEAN(sb),

	       CACHE_SET_META_REPLICAS_HAVE(sb),
	       CACHE_SET_META_REPLICAS_WANT(sb),
	       CACHE_SET_DATA_REPLICAS_HAVE(sb),
	       CACHE_SET_DATA_REPLICAS_WANT(sb),

	       CACHE_SET_META_PREFERRED_CSUM_TYPE(sb) < BCH_CSUM_NR
	       ? bch_csum_types[CACHE_SET_META_PREFERRED_CSUM_TYPE(sb)]
	       : "unknown",

	       CACHE_SET_DATA_PREFERRED_CSUM_TYPE(sb) < BCH_CSUM_NR
	       ? bch_csum_types[CACHE_SET_DATA_PREFERRED_CSUM_TYPE(sb)]
	       : "unknown",

	       CACHE_SET_COMPRESSION_TYPE(sb) < BCH_COMPRESSION_NR
	       ? bch_compression_types[CACHE_SET_COMPRESSION_TYPE(sb)]
	       : "unknown",

	       CACHE_SET_STR_HASH_TYPE(sb) < BCH_STR_HASH_NR
	       ? bch_str_hash_types[CACHE_SET_STR_HASH_TYPE(sb)]
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
		       "  Size:				%s\n"
		       "  Bucket size:			%s\n"
		       "  First bucket:			%u\n"
		       "  Buckets:			%llu\n"
		       "  Last mount:			%s\n"
		       "  State:			%s\n"
		       "  Tier:				%llu\n"
		       "  Has metadata:			%llu\n"
		       "  Has data:			%llu\n"
		       "  Replacement policy:		%s\n"
		       "  Discard:			%llu\n",
		       i, member_uuid_str,
		       pr_units(le16_to_cpu(m->bucket_size) *
				le64_to_cpu(m->nbuckets), units),
		       pr_units(le16_to_cpu(m->bucket_size), units),
		       le16_to_cpu(m->first_bucket),
		       le64_to_cpu(m->nbuckets),
		       last_mount ? ctime(&last_mount) : "(never)",

		       CACHE_STATE(m) < CACHE_STATE_NR
		       ? bch_cache_state[CACHE_STATE(m)]
		       : "unknown",

		       CACHE_TIER(m),
		       CACHE_HAS_METADATA(m),
		       CACHE_HAS_DATA(m),

		       CACHE_REPLACEMENT(m) < CACHE_REPLACEMENT_NR
		       ? bch_cache_replacement_policies[CACHE_REPLACEMENT(m)]
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

	xpread(fd, &sb, sizeof(sb), SB_SECTOR << 9);

	if (memcmp(&sb.magic, &BCACHE_MAGIC, sizeof(sb.magic)))
		die("not a bcache superblock");

	bytes = sizeof(sb) + le16_to_cpu(sb.u64s) * sizeof(u64);

	ret = calloc(1, bytes);

	xpread(fd, ret, bytes, SB_SECTOR << 9);

	return ret;
}
