#ifndef _LIBBCACHE_H
#define _LIBBCACHE_H

#include <linux/uuid.h>
#include "tools-util.h"
#include "vstructs.h"
#include "stdbool.h"

#include "tools-util.h"

struct cache_sb;

enum fsck_err_opts {
	FSCK_ERR_ASK,
	FSCK_ERR_YES,
	FSCK_ERR_NO,
};

extern enum fsck_err_opts fsck_err_opt;

struct dev_opts {
	int		fd;
	const char	*path;
	u64		size; /* 512 byte sectors */
	unsigned	bucket_size;
	unsigned	tier;
	bool		discard;

	u64		first_bucket;
	u64		nbuckets;
};

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
		   uuid_le uuid);

struct bch_sb *bcache_super_read(const char *);

void bcache_super_print(struct bch_sb *, int);

#endif /* _LIBBCACHE_H */
