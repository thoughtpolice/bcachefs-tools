#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <uuid/uuid.h>

#include "linux/bcache.h"
#include "libbcache.h"
#include "checksum.h"
#include "crypto.h"
#include "opts.h"
#include "super-io.h"

#define NSEC_PER_SEC	1000000000L

#define BCH_MIN_NR_NBUCKETS	(1 << 10)

/* first bucket should start 1 mb in, in sectors: */
#define FIRST_BUCKET_OFFSET	(1 << 11)

/* minimum size filesystem we can create, given a bucket size: */
static u64 min_size(unsigned bucket_size)
{
	return (DIV_ROUND_UP(FIRST_BUCKET_OFFSET, bucket_size) +
		BCH_MIN_NR_NBUCKETS) * bucket_size;
}

static void init_layout(struct bch_sb_layout *l)
{
	memset(l, 0, sizeof(*l));

	l->magic		= BCACHE_MAGIC;
	l->layout_type		= 0;
	l->nr_superblocks	= 2;
	l->sb_max_size_bits	= 7;
	l->sb_offset[0]		= cpu_to_le64(BCH_SB_SECTOR);
	l->sb_offset[1]		= cpu_to_le64(BCH_SB_SECTOR +
					      (1 << l->sb_max_size_bits));
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
		   unsigned max_journal_entry_size,
		   char *label,
		   uuid_le uuid)
{
	struct bch_sb *sb;
	struct dev_opts *i;
	struct bch_sb_field_members *mi;
	unsigned u64s, j;

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

	sb = calloc(1, sizeof(*sb) +
		    sizeof(struct bch_sb_field_members) +
		    sizeof(struct bch_member) * nr_devs +
		    sizeof(struct bch_sb_field_crypt));

	sb->version	= cpu_to_le64(BCACHE_SB_VERSION_CDEV_V4);
	sb->magic	= BCACHE_MAGIC;
	sb->block_size	= cpu_to_le16(block_size);
	sb->user_uuid	= uuid;
	sb->nr_devices	= nr_devs;

	init_layout(&sb->layout);

	uuid_generate(sb->uuid.b);

	if (label)
		strncpy((char *) sb->label, label, sizeof(sb->label));

	/*
	 * don't have a userspace crc32c implementation handy, just always use
	 * crc64
	 */
	SET_BCH_SB_CSUM_TYPE(sb,		BCH_CSUM_CRC64);
	SET_BCH_SB_META_CSUM_TYPE(sb,		meta_csum_type);
	SET_BCH_SB_DATA_CSUM_TYPE(sb,		data_csum_type);
	SET_BCH_SB_COMPRESSION_TYPE(sb,		compression_type);

	SET_BCH_SB_BTREE_NODE_SIZE(sb,		btree_node_size);
	SET_BCH_SB_GC_RESERVE(sb,		8);
	SET_BCH_SB_META_REPLICAS_WANT(sb,	meta_replicas);
	SET_BCH_SB_META_REPLICAS_HAVE(sb,	meta_replicas);
	SET_BCH_SB_DATA_REPLICAS_WANT(sb,	data_replicas);
	SET_BCH_SB_DATA_REPLICAS_HAVE(sb,	data_replicas);
	SET_BCH_SB_ERROR_ACTION(sb,		on_error_action);
	SET_BCH_SB_STR_HASH_TYPE(sb,		BCH_STR_HASH_SIPHASH);
	SET_BCH_SB_JOURNAL_ENTRY_SIZE(sb,	ilog2(max_journal_entry_size));

	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now))
		die("error getting current time: %s", strerror(errno));

	sb->time_base_lo	= cpu_to_le64(now.tv_sec * NSEC_PER_SEC + now.tv_nsec);
	sb->time_precision	= cpu_to_le32(1);

	if (passphrase) {
		struct bch_sb_field_crypt *crypt = vstruct_end(sb);

		u64s = sizeof(struct bch_sb_field_crypt) / sizeof(u64);

		le32_add_cpu(&sb->u64s, u64s);
		crypt->field.u64s = cpu_to_le32(u64s);
		crypt->field.type = BCH_SB_FIELD_crypt;

		bch_sb_crypt_init(sb, crypt, passphrase);
		SET_BCH_SB_ENCRYPTION_TYPE(sb, 1);
	}

	mi = vstruct_end(sb);
	u64s = (sizeof(struct bch_sb_field_members) +
		sizeof(struct bch_member) * nr_devs) / sizeof(u64);

	le32_add_cpu(&sb->u64s, u64s);
	mi->field.u64s = cpu_to_le32(u64s);
	mi->field.type = BCH_SB_FIELD_members;

	for (i = devs; i < devs + nr_devs; i++) {
		struct bch_member *m = mi->members + (i - devs);

		uuid_generate(m->uuid.b);
		m->nbuckets	= cpu_to_le64(i->nbuckets);
		m->first_bucket	= cpu_to_le16(i->first_bucket);
		m->bucket_size	= cpu_to_le16(i->bucket_size);

		SET_BCH_MEMBER_TIER(m,		i->tier);
		SET_BCH_MEMBER_REPLACEMENT(m,	CACHE_REPLACEMENT_LRU);
		SET_BCH_MEMBER_DISCARD(m,	i->discard);
	}

	for (i = devs; i < devs + nr_devs; i++) {
		sb->dev_idx = i - devs;

		static const char zeroes[BCH_SB_SECTOR << 9];
		struct nonce nonce = { 0 };

		/* Zero start of disk */
		xpwrite(i->fd, zeroes, BCH_SB_SECTOR << 9, 0);

		xpwrite(i->fd, &sb->layout, sizeof(sb->layout),
			BCH_SB_LAYOUT_SECTOR << 9);

		for (j = 0; j < sb->layout.nr_superblocks; j++) {
			sb->offset = sb->layout.sb_offset[j];

			sb->csum = csum_vstruct(NULL, BCH_SB_CSUM_TYPE(sb),
						   nonce, sb);
			xpwrite(i->fd, sb, vstruct_bytes(sb),
				le64_to_cpu(sb->offset) << 9);
		}

		fsync(i->fd);
		close(i->fd);
	}

	bcache_super_print(sb, HUMAN_READABLE);

	free(sb);
}

struct bch_sb *bcache_super_read(const char *path)
{
	struct bch_sb sb, *ret;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		die("couldn't open %s", path);

	xpread(fd, &sb, sizeof(sb), BCH_SB_SECTOR << 9);

	if (memcmp(&sb.magic, &BCACHE_MAGIC, sizeof(sb.magic)))
		die("not a bcache superblock");

	size_t bytes = vstruct_bytes(&sb);

	ret = malloc(bytes);

	xpread(fd, ret, bytes, BCH_SB_SECTOR << 9);

	return ret;
}

void bcache_super_print(struct bch_sb *sb, int units)
{
	struct bch_sb_field_members *mi;
	char user_uuid_str[40], internal_uuid_str[40], member_uuid_str[40];
	char label[BCH_SB_LABEL_SIZE + 1];
	unsigned i;

	memset(label, 0, sizeof(label));
	memcpy(label, sb->label, sizeof(sb->label));
	uuid_unparse(sb->user_uuid.b, user_uuid_str);
	uuid_unparse(sb->uuid.b, internal_uuid_str);

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
	       pr_units(BCH_SB_BTREE_NODE_SIZE(sb), units),
	       pr_units(1U << BCH_SB_JOURNAL_ENTRY_SIZE(sb), units),

	       BCH_SB_ERROR_ACTION(sb) < BCH_NR_ERROR_ACTIONS
	       ? bch_error_actions[BCH_SB_ERROR_ACTION(sb)]
	       : "unknown",

	       BCH_SB_CLEAN(sb),

	       BCH_SB_META_REPLICAS_HAVE(sb),
	       BCH_SB_META_REPLICAS_WANT(sb),
	       BCH_SB_DATA_REPLICAS_HAVE(sb),
	       BCH_SB_DATA_REPLICAS_WANT(sb),

	       BCH_SB_META_CSUM_TYPE(sb) < BCH_CSUM_NR
	       ? bch_csum_types[BCH_SB_META_CSUM_TYPE(sb)]
	       : "unknown",

	       BCH_SB_DATA_CSUM_TYPE(sb) < BCH_CSUM_NR
	       ? bch_csum_types[BCH_SB_DATA_CSUM_TYPE(sb)]
	       : "unknown",

	       BCH_SB_COMPRESSION_TYPE(sb) < BCH_COMPRESSION_NR
	       ? bch_compression_types[BCH_SB_COMPRESSION_TYPE(sb)]
	       : "unknown",

	       BCH_SB_STR_HASH_TYPE(sb) < BCH_STR_HASH_NR
	       ? bch_str_hash_types[BCH_SB_STR_HASH_TYPE(sb)]
	       : "unknown",

	       BCH_SB_INODE_32BIT(sb),
	       BCH_SB_GC_RESERVE(sb),
	       BCH_SB_ROOT_RESERVE(sb),

	       sb->nr_devices);

	mi = bch_sb_get_members(sb);
	if (!mi) {
		printf("Member info section missing\n");
		return;
	}

	for (i = 0; i < sb->nr_devices; i++) {
		struct bch_member *m = mi->members + i;
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

		       BCH_MEMBER_STATE(m) < BCH_MEMBER_STATE_NR
		       ? bch_cache_state[BCH_MEMBER_STATE(m)]
		       : "unknown",

		       BCH_MEMBER_TIER(m),
		       BCH_MEMBER_HAS_METADATA(m),
		       BCH_MEMBER_HAS_DATA(m),

		       BCH_MEMBER_REPLACEMENT(m) < CACHE_REPLACEMENT_NR
		       ? bch_cache_replacement_policies[BCH_MEMBER_REPLACEMENT(m)]
		       : "unknown",

		       BCH_MEMBER_DISCARD(m));
	}
}
