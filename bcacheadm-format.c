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
	uint64_t	filesystem_size;
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
static uint64_t filesystem_size;
static unsigned tier, replacement_policy;

static uuid_t cache_set_uuid;
static unsigned csum_type = BCH_CSUM_CRC32C;
static unsigned replication_set, meta_replicas = 1, data_replicas = 1;
static unsigned on_error_action;
static int discard;

static uint64_t data_offset = BDEV_DATA_START_DEFAULT;
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
		.label			= strdup(label),
	};
	return 0;
}

static int set_cache_set_uuid(NihOption *option, const char *arg)
{
	if (uuid_parse(arg, cache_set_uuid))
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
	csum_type = read_string_list_or_die(arg, csum_types, "checksum type");
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
	{ 'n',	"btree-node",		N_("Btree node size, default 256k"),
		NULL, "size",	NULL,	set_btree_node_size },
	{ 0,	"fs-size",		N_("Size of filesystem on device" ),
		NULL, "size",	NULL,	set_filesystem_size },

	{ 'p',	"cache_replacement_policy", NULL,
		NULL, "(lru|fifo|random)", NULL, set_replacement_policy },

	{ 0,	"csum-type",		N_("Checksum type"),
		NULL, "(none|crc32c|crc64)", NULL, set_csum_type },

	{ 0,	"on-error",		N_("Action to take on filesystem error"),
		NULL, "(continue|readonly|panic)", NULL, set_on_error_action },

	{ 0,	"discard",		N_("Enable discards"),
		NULL, NULL,	&discard,		NULL },

	{ 't',	"tier",			N_("tier of subsequent devices"),
		NULL, "#",	NULL,	set_tier },

	{ 0,	"replication_set",	N_("replication set of subsequent devices"),
		NULL, "#",	NULL,	set_replication_set },

	{ 0,	"meta-replicas",	N_("number of metadata replicas"),
		NULL, "#",	NULL,	set_meta_replicas },

	{ 0,	"data-replicas",	N_("number of data replicas"),
		NULL, "#",	NULL,	set_data_replicas },

	{ 0,	"cache_mode",		N_("Cache mode (for backing devices)"),
		NULL, "(writethrough|writeback|writearound", NULL, set_cache_mode },

	{ 'o',	"data_offset",		N_("data offset in sectors"),
		NULL, "offset",	&data_offset, NULL},

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

static unsigned ilog2(uint64_t n)
{
	unsigned ret = 0;

	while (n) {
		ret++;
		n >>= 1;
	}

	return ret;
}

static int format_v0(void)
{
	return 0;
}

static int format_v1(void)
{
	struct cache_sb *cache_set_sb;

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
	     i++)
		if (!i->bucket_size) {
			uint64_t size = (i->filesystem_size ?:
					 getblocks(i->fd)) << 9;

			if (size < 1 << 20) /* 1M device - 256 4k buckets*/
				i->bucket_size = rounddown_pow_of_two(size >> 17);
			else
				/* Max 1M bucket at around 256G */
				i->bucket_size = 8 << min((ilog2(size >> 20) / 2), 9U);
		}

	if (!btree_node_size) {
		/* 256k default btree node size */
		btree_node_size = 512;

		for (struct cache_opts *i = cache_devices;
		     i < cache_devices + nr_cache_devices;
		     i++)
			btree_node_size = min(btree_node_size, i->bucket_size);
	}

	cache_set_sb = calloc(1, sizeof(*cache_set_sb) +
			      sizeof(struct cache_member) * nr_cache_devices);

	cache_set_sb->offset		= SB_SECTOR;
	cache_set_sb->version		= BCACHE_SB_VERSION_CDEV_V3;
	cache_set_sb->magic		= BCACHE_MAGIC;
	cache_set_sb->block_size	= block_size;
	uuid_generate(cache_set_sb->set_uuid.b);

	if (uuid_is_null(cache_set_uuid))
		uuid_generate(cache_set_sb->user_uuid.b);
	else
		memcpy(cache_set_sb->user_uuid.b, cache_set_uuid,
		       sizeof(cache_set_sb->user_uuid));

	if (label)
		memcpy(cache_set_sb->label, label, sizeof(cache_set_sb->label));

	/*
	 * don't have a userspace crc32c implementation handy, just always use
	 * crc64
	 */
	SET_CACHE_SB_CSUM_TYPE(cache_set_sb,		BCH_CSUM_CRC64);
	SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb,	csum_type);

	SET_CACHE_BTREE_NODE_SIZE(cache_set_sb,		btree_node_size);
	SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb,	meta_replicas);
	SET_CACHE_SET_META_REPLICAS_HAVE(cache_set_sb,	meta_replicas);
	SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb,	data_replicas);
	SET_CACHE_SET_DATA_REPLICAS_HAVE(cache_set_sb,	data_replicas);
	SET_CACHE_ERROR_ACTION(cache_set_sb,		on_error_action);

	for (struct cache_opts *i = cache_devices;
	     i < cache_devices + nr_cache_devices;
	     i++) {
		if (i->bucket_size < block_size)
			die("Bucket size cannot be smaller than block size");

		struct cache_member *m = cache_set_sb->members + cache_set_sb->nr_in_set++;

		uuid_generate(m->uuid.b);
		m->nbuckets	= (i->filesystem_size ?:
				   getblocks(i->fd)) / i->bucket_size;
		m->first_bucket	= (23 / i->bucket_size) + 3;
		m->bucket_size	= i->bucket_size;

		if (m->nbuckets < 1 << 7)
			die("Not enough buckets: %llu, need %u",
			    m->nbuckets, 1 << 7);

		SET_CACHE_TIER(m,		i->tier);
		SET_CACHE_REPLICATION_SET(m,	i->replication_set);
		SET_CACHE_REPLACEMENT(m,	i->replacement_policy);
		SET_CACHE_DISCARD(m,		discard);
	}

	cache_set_sb->u64s = bch_journal_buckets_offset(cache_set_sb);

	for (unsigned i = 0; i < cache_set_sb->nr_in_set; i++) {
		char uuid_str[40], set_uuid_str[40];
		struct cache_member *m = cache_set_sb->members + i;

		cache_set_sb->disk_uuid		= m->uuid;
		cache_set_sb->nr_this_dev	= i;
		cache_set_sb->csum		= csum_set(cache_set_sb,
						CACHE_SB_CSUM_TYPE(cache_set_sb));

		uuid_unparse(cache_set_sb->disk_uuid.b, uuid_str);
		uuid_unparse(cache_set_sb->user_uuid.b, set_uuid_str);
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
		       (unsigned) cache_set_sb->version,
		       m->nbuckets,
		       cache_set_sb->block_size,
		       m->bucket_size,
		       cache_set_sb->nr_in_set,
		       cache_set_sb->nr_this_dev,
		       m->first_bucket);

		do_write_sb(cache_devices[i].fd, cache_set_sb);
	}

	for (struct backingdev_opts *i = backing_devices;
	     i < backing_devices + nr_backing_devices;
	     i++)
		write_backingdev_sb(i->fd, block_size, cache_mode,
				    data_offset, i->label,
				    cache_set_sb->user_uuid,
				    cache_set_sb->set_uuid);


	return 0;
}

int cmd_format(NihCommand *command, char *const *args)
{
	if (!nr_cache_devices && !nr_backing_devices)
		die("Please supply a device");

	return format_v1();
}
