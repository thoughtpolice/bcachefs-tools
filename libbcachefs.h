#ifndef _LIBBCACHE_H
#define _LIBBCACHE_H

#include <linux/uuid.h>
#include <stdbool.h>

#include "bcachefs_format.h"
#include "tools-util.h"
#include "vstructs.h"

struct format_opts {
	char		*label;
	uuid_le		uuid;

	unsigned	on_error_action;

	unsigned	block_size;
	unsigned	btree_node_size;

	unsigned	meta_replicas;
	unsigned	data_replicas;

	unsigned	meta_replicas_required;
	unsigned	data_replicas_required;

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
		.meta_replicas_required	= 1,
		.data_replicas_required	= 1,
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

void bch2_pick_bucket_size(struct format_opts, struct dev_opts *);
struct bch_sb *bch2_format(struct format_opts, struct dev_opts *, size_t);

void bch2_super_write(int, struct bch_sb *);
struct bch_sb *__bch2_super_read(int, u64);
struct bch_sb *bch2_super_read(const char *);

void bch2_super_print(struct bch_sb *, int);

#endif /* _LIBBCACHE_H */
