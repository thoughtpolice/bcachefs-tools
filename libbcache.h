#ifndef _LIBBCACHE_H
#define _LIBBCACHE_H

#include "util.h"
#include "stdbool.h"

struct dev_opts {
	int		fd;
	const char	*dev;
	u64		size; /* 512 byte sectors */
	unsigned	bucket_size;
	unsigned	tier;
	unsigned	replacement_policy;
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
		   char *label,
		   uuid_le uuid);

void bcache_super_read(const char *, struct cache_sb *);

#endif /* _LIBBCACHE_H */
