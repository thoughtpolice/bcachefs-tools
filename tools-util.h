#ifndef _TOOLS_UTIL_H
#define _TOOLS_UTIL_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/byteorder.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/string.h>
#include <linux/types.h>

#define die(arg, ...)					\
do {							\
	fprintf(stderr, arg "\n", ##__VA_ARGS__);	\
	exit(EXIT_FAILURE);				\
} while (0)

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

#endif /* _TOOLS_UTIL_H */
