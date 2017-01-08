
#include <errno.h>
#include <linux/byteorder.h>
#include <linux/types.h>

/* stub out the bcache code we aren't building: */

struct block_device;
struct bcache_superblock;
struct cache;
struct cache_accounting;
struct cache_set;
struct closure;
struct file;
struct kobject;

struct kmem_cache *bch_search_cache;

const char *bch_backing_dev_register(struct bcache_superblock *sb)
{
	return "not implemented";
}
void bch_blockdevs_stop(struct cache_set *c) {}
int bch_blockdev_volumes_start(struct cache_set *c) { return 0; }
void bch_attach_backing_devs(struct cache_set *c) {}
bool bch_is_open_backing_dev(struct block_device *bdev) { return false; }
void bch_blockdev_exit(void) {}
int bch_blockdev_init(void) { return 0; }

void bch_fs_exit(void) {}
int bch_fs_init(void) { return 0; }

const struct file_operations bch_chardev_fops;

void bcache_dev_sectors_dirty_add(struct cache_set *c, unsigned inode,
				  u64 offset, int nr_sectors) {}
void bch_writeback_recalc_oldest_gens(struct cache_set *c) {}

void bch_notify_cache_set_read_write(struct cache_set *c) {}
void bch_notify_cache_set_read_only(struct cache_set *c) {}
void bch_notify_cache_set_stopped(struct cache_set *c) {}
void bch_notify_cache_read_write(struct cache *c) {}
void bch_notify_cache_read_only(struct cache *c) {}
void bch_notify_cache_added(struct cache *c) {}
void bch_notify_cache_removing(struct cache *c) {}
void bch_notify_cache_removed(struct cache *c) {}
void bch_notify_cache_remove_failed(struct cache *c) {}
void bch_notify_cache_error(struct cache *c, bool b) {}

int bch_cache_accounting_add_kobjs(struct cache_accounting *acc,
				   struct kobject *parent) { return 0; }
void bch_cache_accounting_destroy(struct cache_accounting *acc) {}
void bch_cache_accounting_init(struct cache_accounting *acc,
			       struct closure *parent) {}

//#include "acl.c"
#include "alloc.c"
#include "bkey.c"
#include "bkey_methods.c"
//#include "blockdev.c"
#include "bset.c"
#include "btree_cache.c"
#include "btree_gc.c"
#include "btree_io.c"
#include "btree_iter.c"
#include "btree_update.c"
#include "buckets.c"
//#include "chardev.c"
#include "checksum.c"
#include "clock.c"
#include "closure.c"
#include "compress.c"
#include "debug.c"
#include "dirent.c"
#include "error.c"
#include "extents.c"
//#include "fs.c"
#include "fs-gc.c"
//#include "fs-io.c"
#include "inode.c"
#include "io.c"
#include "journal.c"
#include "keybuf.c"
#include "keylist.c"
#include "migrate.c"
#include "move.c"
#include "movinggc.c"
//#include "notify.c"
#include "opts.c"
//#include "request.c"
#include "siphash.c"
#include "six.c"
//#include "stats.c"
#include "super.c"
//#include "sysfs.c"
#include "tier.c"
#include "trace.c"
#include "util.c"
//#include "writeback.c"
#include "xattr.c"

#define SHIM_KTYPE(type)						\
struct kobj_type type ## _ktype = { .release = type ## _release, }

static void bch_cache_set_internal_release(struct kobject *k) {}

static void bch_cache_set_opts_dir_release(struct kobject *k) {}

static void bch_cache_set_time_stats_release(struct kobject *k) {}

SHIM_KTYPE(bch_cache);
SHIM_KTYPE(bch_cache_set);
SHIM_KTYPE(bch_cache_set_internal);
SHIM_KTYPE(bch_cache_set_time_stats);
SHIM_KTYPE(bch_cache_set_opts_dir);

//#include "tools-util.h"

int cmd_fsck(int argc, char *argv[])
{
	DECLARE_COMPLETION_ONSTACK(shutdown);
	struct cache_set_opts opts = cache_set_opts_empty();
	struct cache_set *c = NULL;
	const char *err;

	printf("registering %s...\n", argv[1]);

	err = bch_register_cache_set(argv + 1, argc - 1, opts, &c);
	if (err) {
		BUG_ON(c);
		fprintf(stderr, "error opening %s: %s\n", argv[1], err);
		exit(EXIT_FAILURE);
	}

	c->stop_completion = &shutdown;
	bch_cache_set_stop(c);
	closure_put(&c->cl);

	/* Killable? */
	wait_for_completion(&shutdown);

	return 0;
}
