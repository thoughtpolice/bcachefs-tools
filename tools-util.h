#ifndef _TOOLS_UTIL_H
#define _TOOLS_UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

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

enum units {
	BYTES,
	SECTORS,
	HUMAN_READABLE,
};

struct units_buf pr_units(u64, enum units);

struct units_buf {
	char	b[20];
};

long strtoul_or_die(const char *, size_t, const char *);

u64 hatoi(const char *);
unsigned hatoi_validate(const char *, const char *);
unsigned nr_args(char * const *);

char *read_file_str(int, const char *);
u64 read_file_u64(int, const char *);

ssize_t read_string_list(const char *, const char * const[]);
ssize_t read_string_list_or_die(const char *, const char * const[],
				const char *);
void print_string_list(const char * const[], size_t);

u64 get_size(const char *, int);
unsigned get_blocksize(const char *, int);

#include "linux/bcache.h"
#include "linux/bcache-ioctl.h"

int bcachectl_open(void);

struct bcache_handle {
	int	ioctl_fd;
	int	sysfs_fd;
};

struct bcache_handle bcache_fs_open(const char *);

bool ask_proceed(void);

#endif /* _TOOLS_UTIL_H */
