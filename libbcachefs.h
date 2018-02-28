#ifndef _LIBBCACHE_H
#define _LIBBCACHE_H

#include <linux/uuid.h>
#include <stdbool.h>

#include "libbcachefs/bcachefs_format.h"
#include "libbcachefs/bcachefs_ioctl.h"
#include "tools-util.h"
#include "libbcachefs/vstructs.h"

struct format_opts {
	char		*label;
	uuid_le		uuid;

	unsigned	on_error_action;

	unsigned	block_size;
	unsigned	btree_node_size;
	unsigned	encoded_extent_max;

	unsigned	meta_replicas;
	unsigned	data_replicas;

	unsigned	meta_replicas_required;
	unsigned	data_replicas_required;

	const char	*foreground_target;
	const char	*background_target;
	const char	*promote_target;

	unsigned	meta_csum_type;
	unsigned	data_csum_type;
	unsigned	compression_type;
	unsigned	background_compression_type;

	bool		encrypted;
	char		*passphrase;
};

static inline struct format_opts format_opts_default()
{
	return (struct format_opts) {
		.on_error_action	= BCH_ON_ERROR_RO,
		.encoded_extent_max	= 128,
		.meta_csum_type		= BCH_CSUM_OPT_CRC32C,
		.data_csum_type		= BCH_CSUM_OPT_CRC32C,
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
	const char	*group;
	unsigned	data_allowed;
	bool		discard;

	u64		nbuckets;

	u64		sb_offset;
	u64		sb_end;
};

static inline struct dev_opts dev_opts_default()
{
	return (struct dev_opts) {
		.data_allowed		= ~0U << 2,
	};
}

void bch2_pick_bucket_size(struct format_opts, struct dev_opts *);
struct bch_sb *bch2_format(struct format_opts, struct dev_opts *, size_t);

void bch2_super_write(int, struct bch_sb *);
struct bch_sb *__bch2_super_read(int, u64);

void bch2_sb_print(struct bch_sb *, bool, unsigned, enum units);

/* ioctl interface: */

int bcachectl_open(void);

struct bchfs_handle {
	uuid_le	uuid;
	int	ioctl_fd;
	int	sysfs_fd;
};

void bcache_fs_close(struct bchfs_handle);
struct bchfs_handle bcache_fs_open(const char *);
struct bchfs_handle bchu_fs_open_by_dev(const char *, unsigned *);

static inline void bchu_disk_add(struct bchfs_handle fs, char *dev)
{
	struct bch_ioctl_disk i = { .dev = (__u64) dev, };

	xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_ADD, &i);
}

static inline void bchu_disk_remove(struct bchfs_handle fs, unsigned dev_idx,
				    unsigned flags)
{
	struct bch_ioctl_disk i = {
		.flags	= flags|BCH_BY_INDEX,
		.dev	= dev_idx,
	};

	xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_REMOVE, &i);
}

static inline void bchu_disk_online(struct bchfs_handle fs, char *dev)
{
	struct bch_ioctl_disk i = { .dev = (__u64) dev, };

	xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_ONLINE, &i);
}

static inline void bchu_disk_offline(struct bchfs_handle fs, unsigned dev_idx,
				     unsigned flags)
{
	struct bch_ioctl_disk i = {
		.flags	= flags|BCH_BY_INDEX,
		.dev	= dev_idx,
	};

	xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_OFFLINE, &i);
}

static inline void bchu_disk_set_state(struct bchfs_handle fs, unsigned dev,
				       unsigned new_state, unsigned flags)
{
	struct bch_ioctl_disk_set_state i = {
		.flags		= flags|BCH_BY_INDEX,
		.new_state	= new_state,
		.dev		= dev,
	};

	xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_SET_STATE, &i);
}

static inline struct bch_ioctl_usage *bchu_usage(struct bchfs_handle fs)
{
	struct bch_ioctl_usage *u = NULL;
	unsigned nr_devices = 4;

	while (1) {
		u = xrealloc(u, sizeof(*u) + sizeof(u->devs[0]) * nr_devices);
		u->nr_devices = nr_devices;

		if (!ioctl(fs.ioctl_fd, BCH_IOCTL_USAGE, u))
			return u;

		if (errno != ENOSPC)
			die("BCH_IOCTL_USAGE error: %m");
		nr_devices *= 2;
	}
}

static inline struct bch_sb *bchu_read_super(struct bchfs_handle fs, unsigned idx)
{
	size_t size = 4096;
	struct bch_sb *sb = NULL;

	while (1) {
		sb = xrealloc(sb, size);
		struct bch_ioctl_read_super i = {
			.size	= size,
			.sb	= (u64) sb,
		};

		if (idx != -1) {
			i.flags |= BCH_READ_DEV|BCH_BY_INDEX;
			i.dev = idx;
		}

		if (!ioctl(fs.ioctl_fd, BCH_IOCTL_READ_SUPER, &i))
			return sb;
		if (errno != ERANGE)
			die("BCH_IOCTL_READ_SUPER error: %m");
		size *= 2;
	}
}

static inline unsigned bchu_disk_get_idx(struct bchfs_handle fs, dev_t dev)
{
	struct bch_ioctl_disk_get_idx i = { .dev = dev };

	return xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_GET_IDX, &i);
}

static inline void bchu_disk_resize(struct bchfs_handle fs,
				    unsigned idx,
				    u64 nbuckets)
{
	struct bch_ioctl_disk_resize i = {
		.flags	= BCH_BY_INDEX,
		.dev	= idx,
		.nbuckets = nbuckets,
	};

	xioctl(fs.ioctl_fd, BCH_IOCTL_DISK_RESIZE, &i);
}

int bchu_data(struct bchfs_handle, struct bch_ioctl_data);

#endif /* _LIBBCACHE_H */
