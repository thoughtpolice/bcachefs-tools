#ifndef _TOOLS_UTIL_H
#define _TOOLS_UTIL_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/bug.h>
#include <linux/byteorder.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/string.h>
#include <linux/types.h>
#include "ccan/darray/darray.h"

#define die(arg, ...)							\
do {									\
	fprintf(stderr, arg "\n", ##__VA_ARGS__);			\
	exit(EXIT_FAILURE);						\
} while (0)

#define mprintf(...)							\
({									\
	char *_str;							\
	asprintf(&_str, __VA_ARGS__);					\
	_str;								\
})

static inline void *xcalloc(size_t count, size_t size)
{
	void *p = calloc(count, size);

	if (!p)
		die("insufficient memory");

	return p;
}

static inline void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (!p)
		die("insufficient memory");

	memset(p, 0, size);
	return p;
}

static inline void xpread(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t r = pread(fd, buf, count, offset);

	if (r != count)
		die("read error (ret %zi)", r);
}

static inline void xpwrite(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t r = pwrite(fd, buf, count, offset);

	if (r != count)
		die("write error (ret %zi err %s)", r, strerror(errno));
}

#define xopenat(_dirfd, _path, ...)					\
({									\
	int _fd = openat((_dirfd), (_path), __VA_ARGS__);		\
	if (_fd < 0)							\
		die("Error opening %s: %s", (_path), strerror(errno));	\
	_fd;								\
})

#define xopen(...)	xopenat(AT_FDCWD, __VA_ARGS__)

static inline struct stat xfstatat(int dirfd, const char *path, int flags)
{
	struct stat stat;
	if (fstatat(dirfd, path, &stat, flags))
		die("stat error: %s", strerror(errno));
	return stat;
}

static inline struct stat xfstat(int fd)
{
	struct stat stat;
	if (fstat(fd, &stat))
		die("stat error: %s", strerror(errno));
	return stat;
}

#define xioctl(_fd, _nr, ...)						\
do {									\
	if (ioctl((_fd), (_nr), ##__VA_ARGS__))				\
		die(#_nr " ioctl error: %s", strerror(errno));		\
} while (0)

enum units {
	BYTES,
	SECTORS,
	HUMAN_READABLE,
};

struct units_buf __pr_units(u64, enum units);

struct units_buf {
	char	b[20];
};

#define pr_units(_v, _u)	__pr_units(_v, _u).b

char *read_file_str(int, const char *);
u64 read_file_u64(int, const char *);

ssize_t read_string_list_or_die(const char *, const char * const[],
				const char *);

u64 get_size(const char *, int);
unsigned get_blocksize(const char *, int);

int bcachectl_open(void);

struct bcache_handle {
	int	ioctl_fd;
	int	sysfs_fd;
};

struct bcache_handle bcache_fs_open(const char *);

bool ask_yn(void);

struct range {
	u64		start;
	u64		end;
};

typedef darray(struct range) ranges;

static inline void range_add(ranges *data, u64 offset, u64 size)
{
	darray_append(*data, (struct range) {
		.start = offset,
		.end = offset + size
	});
}

void ranges_sort_merge(ranges *);
void ranges_roundup(ranges *, unsigned);
void ranges_rounddown(ranges *, unsigned);

struct hole_iter {
	ranges		r;
	size_t		idx;
	u64		end;
};

static inline struct range hole_iter_next(struct hole_iter *iter)
{
	struct range r = {
		.start	= iter->idx ? iter->r.item[iter->idx - 1].end : 0,
		.end	= iter->idx < iter->r.size
			? iter->r.item[iter->idx].start : iter->end,
	};

	BUG_ON(r.start > r.end);

	iter->idx++;
	return r;
}

#define for_each_hole(_iter, _ranges, _end, _i)				\
	for (_iter = (struct hole_iter) { .r = _ranges, .end = _end };	\
	     (_iter.idx <= _iter.r.size &&				\
	      (_i = hole_iter_next(&_iter), true));)

#include <linux/fiemap.h>

struct fiemap_iter {
	struct fiemap		f;
	struct fiemap_extent	fe[1024];
	unsigned		idx;
	int			fd;
};

static inline void fiemap_iter_init(struct fiemap_iter *iter, int fd)
{
	memset(iter, 0, sizeof(*iter));

	iter->f.fm_extent_count	= ARRAY_SIZE(iter->fe);
	iter->f.fm_length	= FIEMAP_MAX_OFFSET;
	iter->fd		= fd;
}

struct fiemap_extent fiemap_iter_next(struct fiemap_iter *);

#define fiemap_for_each(fd, iter, extent)				\
	for (fiemap_iter_init(&iter, fd);				\
	     (extent = fiemap_iter_next(&iter)).fe_length;)

const char *strcmp_prefix(const char *, const char *);

#endif /* _TOOLS_UTIL_H */
