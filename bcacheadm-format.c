/*
 * Authors: Kent Overstreet <kmo@daterainc.com>
 *	    Gabriel de Perthuis <g2p.code@gmail.com>
 *	    Jacob Malevich <jam@datera.io>
 *
 * GPLv2
 */

#if 0
#include <nih/main.h>
#include <nih/logging.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <blkid.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <uuid/uuid.h>

#include <nih/command.h>
#include <nih/option.h>

#include "bcache.h"
#include "bcacheadm-format.h"

static struct cache_opts {
	int		fd;
	const char	*dev;
	unsigned	bucket_size;
	unsigned	tier;
	unsigned	replacement_policy;
	unsigned	replication_set;
	u64		filesystem_size;

	u64		first_bucket;
	u64		nbuckets;
} cache_devices[MAX_DEVS];

static struct backingdev_opts {
	int		fd;
	const char	*dev;
	const char	*label;
} backing_devices[MAX_DEVS];

static size_t nr_backing_devices = 0, nr_cache_devices = 0;

static char *label = NULL;

/* All in units of 512 byte sectors */
static unsigned block_size, bucket_size, btree_node_size;
static u64 filesystem_size;
static unsigned tier, replacement_policy;

static uuid_le set_uuid, user_uuid;
static unsigned meta_csum_type = BCH_CSUM_CRC32C;
static unsigned data_csum_type = BCH_CSUM_CRC32C;
static unsigned compression_type = BCH_COMPRESSION_NONE;

static unsigned replication_set, meta_replicas = 1, data_replicas = 1;
static unsigned on_error_action;
static int discard;
static unsigned version = 1;

static u64 data_offset = BDEV_DATA_START_DEFAULT;
static unsigned cache_mode = CACHE_MODE_WRITEBACK;

static int set_cache(NihOption *option, const char *arg)
{
	cache_devices[nr_cache_devices++] = (struct cache_opts) {
		.fd			= dev_open(arg),
		.dev			= strdup(arg),
		.bucket_size		= bucket_size,
		.tier			= tier,
		.replacement_policy	= replacement_policy,
		.replication_set	= replication_set,
		.filesystem_size	= filesystem_size,
	};
	return 0;
}

static int set_bdev(NihOption *option, const char *arg)
{
	backing_devices[nr_backing_devices++] = (struct backingdev_opts) {
		.fd			= dev_open(arg),
		.dev			= strdup(arg),
		.label			= label ? strdup(label) : NULL,
	};
	return 0;
}

static int set_cache_set_uuid(NihOption *option, const char *arg)
{
	if (uuid_parse(arg, user_uuid.b))
		die("Bad uuid");
	return 0;
}

static int set_block_size(NihOption *option, const char *arg)
{
	block_size = hatoi_validate(arg, "block size");
	return 0;
}

static int set_bucket_sizes(NihOption *option, const char *arg)
{
	bucket_size = hatoi_validate(arg, "bucket size");
	return 0;
}

static int set_btree_node_size(NihOption *option, const char *arg)
{
	btree_node_size = hatoi_validate(arg, "btree node size");
	return 0;
}

static int set_filesystem_size(NihOption *option, const char *arg)
{
	filesystem_size = hatoi(arg) >> 9;
	return 0;
}

static int set_replacement_policy(NihOption *option, const char *arg)
{
	replacement_policy = read_string_list_or_die(arg, replacement_policies,
						     "replacement policy");
	return 0;
}

static int set_csum_type(NihOption *option, const char *arg)
{
	unsigned *csum_type = option->value;

	*csum_type = read_string_list_or_die(arg, csum_types, "checksum type");
	return 0;
}

static int set_compression_type(NihOption *option, const char *arg)
{
	compression_type = read_string_list_or_die(arg, compression_types,
						   "compression type");
	return 0;
}

static int set_on_error_action(NihOption *option, const char *arg)
{
	on_error_action = read_string_list_or_die(arg, error_actions,
						  "error action");
	return 0;
}

static int set_tier(NihOption *option, const char *arg)
{
	tier = strtoul_or_die(arg, CACHE_TIERS, "tier");
	return 0;
}

static int set_replication_set(NihOption *option, const char *arg)
{
	replication_set = strtoul_or_die(arg, CACHE_REPLICATION_SET_MAX,
					 "replication set");
	return 0;
}

static int set_meta_replicas(NihOption *option, const char *arg)
{
	meta_replicas = strtoul_or_die(arg, CACHE_SET_META_REPLICAS_WANT_MAX,
				       "meta_replicas");
	return 0;
}

static int set_data_replicas(NihOption *option, const char *arg)
{
	data_replicas = strtoul_or_die(arg, CACHE_SET_DATA_REPLICAS_WANT_MAX,
				       "data_replicas");
	return 0;
}

static int set_cache_mode(NihOption *option, const char *arg)
{
	cache_mode = read_string_list_or_die(arg, bdev_cache_mode,
					     "cache mode");
	return 0;
}

static int set_version(NihOption *option, const char *arg)
{
	version = strtoul_or_die(arg, 2, "version");
	return 0;
}

NihOption opts_format[] = {
//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}

	{ 'C',	"cache",		N_("Format a cache device"),
		NULL, "dev",	NULL,	set_cache },
	{ 'B',	"bdev",			N_("Format a backing device"),
		NULL, "dev",	NULL,	set_bdev },

	{ 'l',	"label",		N_("label"),
		NULL, "label",	&label, NULL},
	{ 0,	"cset_uuid",		N_("UUID for the cache set"),
		NULL, "uuid",	NULL,	set_cache_set_uuid },

	{ 'w',	"block",		N_("block size (hard sector size of SSD, often 2k"),
		NULL, "size",	NULL,	set_block_size },
	{ 'b',	"bucket",		N_("bucket size"),
		NULL, "size",	NULL,	set_bucket_sizes },
	{ 'n',	"btree_node",		N_("Btree node size, default 256k"),
		NULL, "size",	NULL,	set_btree_node_size },
	{ 0,	"fs_size",		N_("Size of filesystem on device" ),
		NULL, "size",	NULL,	set_filesystem_size },

	{ 'p',	"cache_replacement_policy", NULL,
		NULL, "(lru|fifo|random)", NULL, set_replacement_policy },

	{ 0,	"metadata_csum_type",	N_("Checksum type"),
		NULL, "(none|crc32c|crc64)", &meta_csum_type, set_csum_type },

	{ 0,	"data_csum_type",	N_("Checksum type"),
		NULL, "(none|crc32c|crc64)", &data_csum_type, set_csum_type },

	{ 0,	"compression_type",	N_("Checksum type"),
		NULL, "(none|crc32c|crc64)", NULL, set_compression_type },

	{ 0,	"error_action",		N_("Action to take on filesystem error"),
		NULL, "(continue|readonly|panic)", NULL, set_on_error_action },

	{ 0,	"discard",		N_("Enable discards"),
		NULL, NULL,		&discard,	NULL },

	{ 't',	"tier",			N_("tier of subsequent devices"),
		NULL, "#",	NULL,	set_tier },

	{ 0,	"replication_set",	N_("replication set of subsequent devices"),
		NULL, "#",	NULL,	set_replication_set },

	{ 0,	"meta_replicas",	N_("number of metadata replicas"),
		NULL, "#",	NULL,	set_meta_replicas },

	{ 0,	"data_replicas",	N_("number of data replicas"),
		NULL, "#",	NULL,	set_data_replicas },

	{ 0,	"cache_mode",		N_("Cache mode (for backing devices)"),
		NULL, "(writethrough|writeback|writearound", NULL, set_cache_mode },

	{ 'o',	"data_offset",		N_("data offset in sectors"),
		NULL, "offset",	&data_offset, NULL},

	{ 'v',	"version",		N_("superblock version"),
		NULL, "#",	NULL,	set_version},

	NIH_OPTION_LAST
};

static unsigned rounddown_pow_of_two(unsigned n)
{
	unsigned ret;

	do {
		ret = n;
		n &= n - 1;
	} while (n);

	return ret;
}

static unsigned ilog2(u64 n)
{
	unsigned ret = 0;

	while (n) {
		ret++;
		n >>= 1;
	}

	return ret;
}

void __do_write_sb(int fd, void *sb, size_t bytes)
{
	char zeroes[SB_START] = {0};

	/* Zero start of disk */
	if (pwrite(fd, zeroes, SB_START, 0) != SB_START) {
		perror("write error trying to zero start of disk\n");
		exit(EXIT_FAILURE);
	}
	/* Write superblock */
	if (pwrite(fd, sb, bytes, SB_START) != bytes) {
		perror("write error trying to write superblock\n");
		exit(EXIT_FAILURE);
	}

	fsync(fd);
	close(fd);
}

#define do_write_sb(_fd, _sb)			\
	__do_write_sb(_fd, _sb, ((void *) __bset_bkey_last(_sb)) - (void *) _sb);

void write_backingdev_sb(int fd, unsigned block_size, unsigned mode,
			 u64 data_offset, const char *label,
			 uuid_le set_uuid)
{
	char uuid_str[40];
	struct cache_sb sb;

	memset(&sb, 0, sizeof(struct cache_sb));

	sb.offset	= SB_SECTOR;
	sb.version	= BCACHE_SB_VERSION_BDEV;
	sb.magic	= BCACHE_MAGIC;
	uuid_generate(sb.disk_uuid.b);
	sb.set_uuid	= set_uuid;
	sb.block_size	= block_size;

	uuid_unparse(sb.disk_uuid.b, uuid_str);
	if (label)
		memcpy(sb.label, label, SB_LABEL_SIZE);

	SET_BDEV_CACHE_MODE(&sb, mode);

	if (data_offset != BDEV_DATA_START_DEFAULT) {
		sb.version = BCACHE_SB_VERSION_BDEV_WITH_OFFSET;
		sb.bdev_data_offset = data_offset;
	}

	sb.csum = csum_set(&sb, BCH_CSUM_CRC64);

	printf("UUID:			%s\n"
	       "version:		%u\n"
	       "block_size:		%u\n"
	       "data_offset:		%llu\n",
	       uuid_str, (unsigned) sb.version,
	       sb.block_size, data_offset);

	do_write_sb(fd, &sb);
}

static void format_v0(void)
{
	set_uuid = user_uuid;

	for (struct cache_opts *i = cache_devices;
	     i < cache_devices + nr_cache_devices;
	     i++)
		bucket_size = min(bucket_size, i->bucket_size);

	struct cache_sb_v0 *sb = calloc(1, sizeof(*sb));

	sb->offset		= SB_SECTOR;
	sb->version		= BCACHE_SB_VERSION_CDEV_WITH_UUID;
	sb->magic		= BCACHE_MAGIC;
	sb->block_size	= block_size;
	sb->bucket_size	= bucket_size;
	sb->set_uuid		= set_uuid;
	sb->nr_in_set		= nr_cache_devices;

	if (label)
		memcpy(sb->label, label, sizeof(sb->label));

	for (struct cache_opts *i = cache_devices;
	     i < cache_devices + nr_cache_devices;
	     i++) {
		char uuid_str[40], set_uuid_str[40];

		uuid_generate(sb->uuid.b);
		sb->nbuckets		= i->nbuckets;
		sb->first_bucket	= i->first_bucket;
		sb->nr_this_dev		= i - cache_devices;
		sb->csum		= csum_set(sb, BCH_CSUM_CRC64);

		uuid_unparse(sb->uuid.b, uuid_str);
		uuid_unparse(sb->set_uuid.b, set_uuid_str);
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
		       sb->nbuckets,
		       sb->block_size,
		       sb->bucket_size,
		       sb->nr_in_set,
		       sb->nr_this_dev,
		       sb->first_bucket);

		do_write_sb(i->fd, sb);
	}
}

static void format_v1(void)
{
	struct cache_sb *sb;

	sb = calloc(1, sizeof(*sb) + sizeof(struct cache_member) *
		    nr_cache_devices);

	sb->offset		= SB_SECTOR;
	sb->version		= BCACHE_SB_VERSION_CDEV_V3;
	sb->magic		= BCACHE_MAGIC;
	sb->block_size	= block_size;
	sb->set_uuid		= set_uuid;
	sb->user_uuid		= user_uuid;

	if (label)
		memcpy(sb->label, label, sizeof(sb->label));

	/*
	 * don't have a userspace crc32c implementation handy, just always use
	 * crc64
	 */
	SET_CACHE_SB_CSUM_TYPE(sb,		BCH_CSUM_CRC64);
	SET_CACHE_META_PREFERRED_CSUM_TYPE(sb,	meta_csum_type);
	SET_CACHE_DATA_PREFERRED_CSUM_TYPE(sb,	data_csum_type);
	SET_CACHE_COMPRESSION_TYPE(sb,		compression_type);

	SET_CACHE_BTREE_NODE_SIZE(sb,		btree_node_size);
	SET_CACHE_SET_META_REPLICAS_WANT(sb,	meta_replicas);
	SET_CACHE_SET_META_REPLICAS_HAVE(sb,	meta_replicas);
	SET_CACHE_SET_DATA_REPLICAS_WANT(sb,	data_replicas);
	SET_CACHE_SET_DATA_REPLICAS_HAVE(sb,	data_replicas);
	SET_CACHE_ERROR_ACTION(sb,		on_error_action);

	for (struct cache_opts *i = cache_devices;
	     i < cache_devices + nr_cache_devices;
	     i++) {
		struct cache_member *m = sb->members + sb->nr_in_set++;

		uuid_generate(m->uuid.b);
		m->nbuckets	= i->nbuckets;
		m->first_bucket	= i->first_bucket;
		m->bucket_size	= i->bucket_size;

		if (m->nbuckets < 1 << 7)
			die("Not enough buckets: %llu, need %u",
			    m->nbuckets, 1 << 7);

		SET_CACHE_TIER(m,		i->tier);
		SET_CACHE_REPLICATION_SET(m,	i->replication_set);
		SET_CACHE_REPLACEMENT(m,	i->replacement_policy);
		SET_CACHE_DISCARD(m,		discard);
	}

	sb->u64s = bch_journal_buckets_offset(sb);

	for (unsigned i = 0; i < sb->nr_in_set; i++) {
		char uuid_str[40], set_uuid_str[40];
		struct cache_member *m = sb->members + i;

		sb->disk_uuid		= m->uuid;
		sb->nr_this_dev	= i;
		sb->csum		= csum_set(sb,
						CACHE_SB_CSUM_TYPE(sb));

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
		       m->nbuckets,
		       sb->block_size,
		       m->bucket_size,
		       sb->nr_in_set,
		       sb->nr_this_dev,
		       m->first_bucket);

		do_write_sb(cache_devices[i].fd, sb);
	}
}

int cmd_format(NihCommand *command, char *const *args)
{
	if (!nr_cache_devices && !nr_backing_devices)
		die("Please supply a device");

	if (uuid_is_null(user_uuid.b))
		uuid_generate(user_uuid.b);

	uuid_generate(set_uuid.b);

	if (!block_size) {
		for (struct cache_opts *i = cache_devices;
		     i < cache_devices + nr_cache_devices;
		     i++)
			block_size = max(block_size, get_blocksize(i->dev, i->fd));

		for (struct backingdev_opts *i = backing_devices;
		     i < backing_devices + nr_backing_devices;
		     i++)
			block_size = max(block_size, get_blocksize(i->dev, i->fd));
	}

	for (struct cache_opts *i = cache_devices;
	     i < cache_devices + nr_cache_devices;
	     i++) {
		if (!i->bucket_size) {
			u64 size = (i->filesystem_size ?:
				    getblocks(i->fd)) << 9;

			if (size < 1 << 20) /* 1M device - 256 4k buckets*/
				i->bucket_size = rounddown_pow_of_two(size >> 17);
			else
				/* Max 1M bucket at around 256G */
				i->bucket_size = 8 << min((ilog2(size >> 20) / 2), 9U);
		}

		if (i->bucket_size < block_size)
			die("Bucket size cannot be smaller than block size");

		i->nbuckets	= (i->filesystem_size ?:
				   getblocks(i->fd)) / i->bucket_size;
		i->first_bucket	= (23 / i->bucket_size) + 3;

		if (i->nbuckets < 1 << 7)
			die("Not enough buckets: %llu, need %u",
			    i->nbuckets, 1 << 7);
	}

	if (!btree_node_size) {
		/* 256k default btree node size */
		btree_node_size = 512;

		for (struct cache_opts *i = cache_devices;
		     i < cache_devices + nr_cache_devices;
		     i++)
			btree_node_size = min(btree_node_size, i->bucket_size);
	}

	switch (version) {
	case 0:
		format_v0();
		break;
	case 1:
		format_v1();
		break;
	}

	for (struct backingdev_opts *i = backing_devices;
	     i < backing_devices + nr_backing_devices;
	     i++)
		write_backingdev_sb(i->fd, block_size, cache_mode,
				    data_offset, i->label,
				    set_uuid);

	return 0;
}
