#ifndef _LIBBCACHE_H
#define _LIBBCACHE_H

#include <linux/bcache.h>
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

struct format_opts {
	char		*label;
	uuid_le		uuid;

	unsigned	on_error_action;
	unsigned	max_journal_entry_size; /* will be removed */

	unsigned	block_size;
	unsigned	btree_node_size;

	unsigned	meta_replicas;
	unsigned	data_replicas;

	unsigned	meta_csum_type;
	unsigned	data_csum_type;
	unsigned	compression_type;

	bool		encrypted;
	char		*passphrase;
};

static inline struct format_opts format_opts_default()
{
	return (struct format_opts) {
		.on_error_action	= BCH_ON_ERROR_RO,
		.meta_csum_type		= BCH_CSUM_CRC32C,
		.data_csum_type		= BCH_CSUM_CRC32C,
		.meta_replicas		= 1,
		.data_replicas		= 1,
	};
}

struct dev_opts {
	int		fd;
	char		*path;
	u64		size; /* 512 byte sectors */
	unsigned	bucket_size;
	unsigned	tier;
	bool		discard;

	u64		nbuckets;

	u64		sb_offset;
	u64		sb_end;
};

struct bch_sb *bcache_format(struct format_opts, struct dev_opts *, size_t);

void bcache_super_write(int, struct bch_sb *);
struct bch_sb *__bcache_super_read(int, u64);
struct bch_sb *bcache_super_read(const char *);

void bcache_super_print(struct bch_sb *, int);

#endif /* _LIBBCACHE_H */
