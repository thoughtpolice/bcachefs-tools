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

/* minimum size filesystem we can create, given a bucket size: */
static u64 min_size(unsigned bucket_size)
{
	return BCH_MIN_NR_NBUCKETS * bucket_size;
}

static void init_layout(struct bch_sb_layout *l, unsigned block_size,
			u64 start, u64 end)
{
	unsigned sb_size;
	u64 backup; /* offset of 2nd sb */

	memset(l, 0, sizeof(*l));

	if (start != BCH_SB_SECTOR)
		start = round_up(start, block_size);
	end = round_down(end, block_size);

	if (start >= end)
		die("insufficient space for superblocks");

	/*
	 * Create two superblocks in the allowed range: reserve a maximum of 64k
	 */
	sb_size = min_t(u64, 128, end - start / 2);

	backup = start + sb_size;
	backup = round_up(backup, block_size);

	backup = min(backup, end);

	sb_size = min(end - backup, backup- start);
	sb_size = rounddown_pow_of_two(sb_size);

	if (sb_size < 8)
		die("insufficient space for superblocks");

	l->magic		= BCACHE_MAGIC;
	l->layout_type		= 0;
	l->nr_superblocks	= 2;
	l->sb_max_size_bits	= ilog2(sb_size);
	l->sb_offset[0]		= cpu_to_le64(start);
	l->sb_offset[1]		= cpu_to_le64(backup);
}

struct bch_sb *bcache_format(struct format_opts opts,
			     struct dev_opts *devs, size_t nr_devs)
{
	struct bch_sb *sb;
	struct dev_opts *i;
	struct bch_sb_field_members *mi;
	unsigned u64s;

	/* calculate block size: */
	if (!opts.block_size)
		for (i = devs; i < devs + nr_devs; i++)
			opts.block_size = max(opts.block_size,
					      get_blocksize(i->path, i->fd));

	/* calculate bucket sizes: */
	for (i = devs; i < devs + nr_devs; i++) {
		if (!i->sb_offset) {
			i->sb_offset	= BCH_SB_SECTOR;
			i->sb_end	= BCH_SB_SECTOR + 256;
		}

		if (!i->size)
			i->size = get_size(i->path, i->fd) >> 9;

		if (!i->bucket_size) {
			if (i->size < min_size(opts.block_size))
				die("cannot format %s, too small (%llu sectors, min %llu)",
				    i->path, i->size, min_size(opts.block_size));

			/* Want a bucket size of at least 128k, if possible: */
			i->bucket_size = max(opts.block_size, 256U);

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

		i->nbuckets	= i->size / i->bucket_size;

		if (i->bucket_size < opts.block_size)
			die("Bucket size cannot be smaller than block size");

		if (i->nbuckets < BCH_MIN_NR_NBUCKETS)
			die("Not enough buckets: %llu, need %u (bucket size %u)",
			    i->nbuckets, BCH_MIN_NR_NBUCKETS, i->bucket_size);
	}

	/* calculate btree node size: */
	if (!opts.btree_node_size) {
		/* 256k default btree node size */
		opts.btree_node_size = 512;

		for (i = devs; i < devs + nr_devs; i++)
			opts.btree_node_size =
				min(opts.btree_node_size, i->bucket_size);
	}

	if (!opts.max_journal_entry_size) {
		/* 2 MB default: */
		opts.max_journal_entry_size = 4096;
	}

	opts.max_journal_entry_size =
		roundup_pow_of_two(opts.max_journal_entry_size);

	if (uuid_is_null(opts.uuid.b))
		uuid_generate(opts.uuid.b);

	sb = calloc(1, sizeof(*sb) +
		    sizeof(struct bch_sb_field_members) +
		    sizeof(struct bch_member) * nr_devs +
		    sizeof(struct bch_sb_field_crypt));

	sb->version	= cpu_to_le64(BCACHE_SB_VERSION_CDEV_V4);
	sb->magic	= BCACHE_MAGIC;
	sb->block_size	= cpu_to_le16(opts.block_size);
	sb->user_uuid	= opts.uuid;
	sb->nr_devices	= nr_devs;

	uuid_generate(sb->uuid.b);

	if (opts.label)
		strncpy((char *) sb->label, opts.label, sizeof(sb->label));

	SET_BCH_SB_CSUM_TYPE(sb,		opts.meta_csum_type);
	SET_BCH_SB_META_CSUM_TYPE(sb,		opts.meta_csum_type);
	SET_BCH_SB_DATA_CSUM_TYPE(sb,		opts.data_csum_type);
	SET_BCH_SB_COMPRESSION_TYPE(sb,		opts.compression_type);

	SET_BCH_SB_BTREE_NODE_SIZE(sb,		opts.btree_node_size);
	SET_BCH_SB_GC_RESERVE(sb,		8);
	SET_BCH_SB_META_REPLICAS_WANT(sb,	opts.meta_replicas);
	SET_BCH_SB_META_REPLICAS_HAVE(sb,	opts.meta_replicas);
	SET_BCH_SB_DATA_REPLICAS_WANT(sb,	opts.data_replicas);
	SET_BCH_SB_DATA_REPLICAS_HAVE(sb,	opts.data_replicas);
	SET_BCH_SB_ERROR_ACTION(sb,		opts.on_error_action);
	SET_BCH_SB_STR_HASH_TYPE(sb,		BCH_STR_HASH_SIPHASH);
	SET_BCH_SB_JOURNAL_ENTRY_SIZE(sb,	ilog2(opts.max_journal_entry_size));

	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now))
		die("error getting current time: %s", strerror(errno));

	sb->time_base_lo	= cpu_to_le64(now.tv_sec * NSEC_PER_SEC + now.tv_nsec);
	sb->time_precision	= cpu_to_le32(1);

	if (opts.encrypted) {
		struct bch_sb_field_crypt *crypt = vstruct_end(sb);

		u64s = sizeof(struct bch_sb_field_crypt) / sizeof(u64);

		le32_add_cpu(&sb->u64s, u64s);
		crypt->field.u64s = cpu_to_le32(u64s);
		crypt->field.type = BCH_SB_FIELD_crypt;

		bch_sb_crypt_init(sb, crypt, opts.passphrase);
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
		m->first_bucket	= 0;
		m->bucket_size	= cpu_to_le16(i->bucket_size);

		SET_BCH_MEMBER_TIER(m,		i->tier);
		SET_BCH_MEMBER_REPLACEMENT(m,	CACHE_REPLACEMENT_LRU);
		SET_BCH_MEMBER_DISCARD(m,	i->discard);
	}

	for (i = devs; i < devs + nr_devs; i++) {
		sb->dev_idx = i - devs;

		init_layout(&sb->layout, opts.block_size,
			    i->sb_offset, i->sb_end);

		if (i->sb_offset == BCH_SB_SECTOR) {
			/* Zero start of disk */
			static const char zeroes[BCH_SB_SECTOR << 9];

			xpwrite(i->fd, zeroes, BCH_SB_SECTOR << 9, 0);
		}

		bcache_super_write(i->fd, sb);
		close(i->fd);
	}

	return sb;
}

void bcache_super_write(int fd, struct bch_sb *sb)
{
	struct nonce nonce = { 0 };

	for (unsigned i = 0; i < sb->layout.nr_superblocks; i++) {
		sb->offset = sb->layout.sb_offset[i];

		if (sb->offset == BCH_SB_SECTOR) {
			/* Write backup layout */
			xpwrite(fd, &sb->layout, sizeof(sb->layout),
				BCH_SB_LAYOUT_SECTOR << 9);
		}

		sb->csum = csum_vstruct(NULL, BCH_SB_CSUM_TYPE(sb), nonce, sb);
		xpwrite(fd, sb, vstruct_bytes(sb),
			le64_to_cpu(sb->offset) << 9);
	}

	fsync(fd);
}

struct bch_sb *__bcache_super_read(int fd, u64 sector)
{
	struct bch_sb sb, *ret;

	xpread(fd, &sb, sizeof(sb), sector << 9);

	if (memcmp(&sb.magic, &BCACHE_MAGIC, sizeof(sb.magic)))
		die("not a bcache superblock");

	size_t bytes = vstruct_bytes(&sb);

	ret = malloc(bytes);

	xpread(fd, ret, bytes, sector << 9);

	return ret;
}

struct bch_sb *bcache_super_read(const char *path)
{
	int fd = xopen(path, O_RDONLY);
	struct bch_sb *sb = __bcache_super_read(fd, BCH_SB_SECTOR);
	close(fd);
	return sb;
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
		       ? bch_dev_state[BCH_MEMBER_STATE(m)]
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
