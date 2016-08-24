#ifndef _UTIL_H
#define _UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/* linux kernel style types: */

#include <asm/types.h>

typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef __s8	s8;
typedef __s16	s16;
typedef __s32	s32;
typedef __s64	s64;

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define die(arg, ...)					\
do {							\
	fprintf(stderr, arg "\n", ##__VA_ARGS__);	\
	exit(EXIT_FAILURE);				\
} while (0)

u64 rounddown_pow_of_two(u64);
unsigned ilog2(u64);

char *skip_spaces(const char *str);
char *strim(char *s);

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

#include "bcache-ondisk.h"
#include "bcache-ioctl.h"

u64 bch_checksum(unsigned, const void *, size_t);

#define __bkey_idx(_set, _offset)					\
	((_set)->_data + (_offset))

#define __bset_bkey_last(_set)						\
	 __bkey_idx((_set), (_set)->u64s)

#define __csum_set(i, u64s, type)					\
({									\
	const void *start = ((const void *) (i)) + sizeof(i->csum);	\
	const void *end = __bkey_idx(i, u64s);				\
									\
	bch_checksum(type, start, end - start);				\
})

#define csum_set(i, type)	__csum_set(i, (i)->u64s, type)

int bcachectl_open(void);

#include <dirent.h>

struct bcache_handle {
	DIR	*sysfs;
	int	fd;
};

struct bcache_handle bcache_fs_open(const char *);

bool ask_proceed(void);

void memzero_explicit(void *, size_t);

struct nih_option;
char **bch_nih_init(int argc, char *argv[], struct nih_option *options);

#endif /* _UTIL_H */
