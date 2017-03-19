
#include <errno.h>
#include <linux/types.h>

#define bch2_fmt(_c, fmt)	fmt "\n"

#include "libbcachefs.h"
#include "tools-util.h"

enum fsck_err_opts fsck_err_opt;

/* Returns true if error should be fixed: */

/* XXX: flag if we ignore errors */

/*
 * If it's an error that we can't ignore, and we're running non
 * interactively - return true and have the error fixed so that we don't have to
 * bail out and stop the fsck early, so that the user can see all the errors
 * present:
 */
#define __fsck_err(c, _can_fix, _can_ignore, _nofix_msg, msg, ...)	\
({									\
	bool _fix = false;						\
									\
	if (_can_fix) {							\
		switch (fsck_err_opt) {					\
		case FSCK_ERR_ASK:					\
			printf(msg ": fix?", ##__VA_ARGS__);		\
			_fix = ask_yn();				\
									\
			break;						\
		case FSCK_ERR_YES:					\
			bch_err(c, msg ", fixing", ##__VA_ARGS__);	\
			_fix = true;					\
			break;						\
		case FSCK_ERR_NO:					\
			bch_err(c, msg, ##__VA_ARGS__);			\
			_fix = false;					\
			break;						\
		}							\
	} else if (_can_ignore) {					\
		bch_err(c, msg, ##__VA_ARGS__);				\
	}								\
									\
	if (_can_fix && !_can_ignore && fsck_err_opt == FSCK_ERR_NO)	\
		_fix = true;						\
									\
	if (!_fix && !_can_ignore) {					\
		printf("Fatal filesystem inconsistency, halting\n");	\
		ret = BCH_FSCK_ERRORS_NOT_FIXED;			\
		goto fsck_err;						\
	}								\
									\
	_fix;								\
})

//#include "acl.c"
#include "alloc.c"
#include "bkey.c"
#include "bkey_methods.c"
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
#include "keylist.c"
#include "migrate.c"
#include "move.c"
#include "movinggc.c"
#include "opts.c"
#include "siphash.c"
#include "six.c"
#include "super.c"
#include "super-io.c"
//#include "sysfs.c"
#include "tier.c"
#include "trace.c"
#include "util.c"
#include "xattr.c"

#define SHIM_KTYPE(type)						\
struct kobj_type type ## _ktype = { .release = type ## _release, }

static void bch2_fs_internal_release(struct kobject *k) {}

static void bch2_fs_opts_dir_release(struct kobject *k) {}

static void bch2_fs_time_stats_release(struct kobject *k) {}

SHIM_KTYPE(bch2_dev);
SHIM_KTYPE(bch2_fs);
SHIM_KTYPE(bch2_fs_internal);
SHIM_KTYPE(bch2_fs_time_stats);
SHIM_KTYPE(bch2_fs_opts_dir);
