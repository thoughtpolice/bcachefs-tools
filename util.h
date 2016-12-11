#ifndef _UTIL_H
#define _UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/* linux kernel style types: */

#include <asm/types.h>
#include <asm/byteorder.h>

typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef __s8	s8;
typedef __s16	s16;
typedef __s32	s32;
typedef __s64	s64;

#define cpu_to_le16		__cpu_to_le16
#define cpu_to_le32		__cpu_to_le32
#define cpu_to_le64		__cpu_to_le64

#define le16_to_cpu		__le16_to_cpu
#define le32_to_cpu		__le32_to_cpu
#define le64_to_cpu		__le64_to_cpu

static inline void le16_add_cpu(__le16 *var, u16 val)
{
	*var = cpu_to_le16(le16_to_cpu(*var) + val);
}

static inline void le32_add_cpu(__le32 *var, u32 val)
{
	*var = cpu_to_le32(le32_to_cpu(*var) + val);
}

static inline void le64_add_cpu(__le64 *var, u64 val)
{
	*var = cpu_to_le64(le64_to_cpu(*var) + val);
}

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

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

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

#define die(arg, ...)					\
do {							\
	fprintf(stderr, arg "\n", ##__VA_ARGS__);	\
	exit(EXIT_FAILURE);				\
} while (0)

unsigned ilog2(u64);
u64 rounddown_pow_of_two(u64);
u64 roundup_pow_of_two(u64);

char *skip_spaces(const char *str);
char *strim(char *s);

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

#endif /* _UTIL_H */
