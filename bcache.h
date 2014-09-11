/*
 * Author: Kent Overstreet <kmo@daterainc.com>
 *
 * GPLv2
 */

#ifndef _BCACHE_H
#define _BCACHE_H

#include <linux/bcache.h>

typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef __s8	s8;
typedef __s16	s16;
typedef __s32	s32;
typedef __s64	s64;

#define SB_START		(SB_SECTOR * 512)

extern const char * const cache_state[];
extern const char * const replacement_policies[];
extern const char * const csum_types[];
extern const char * const bdev_cache_mode[];
extern const char * const bdev_state[];

ssize_t read_string_list(const char *, const char * const[]);
ssize_t read_string_list_or_die(const char *, const char * const[],
				const char *);
void print_string_list(const char * const[], size_t);

uint64_t bch_checksum(unsigned, const void *, size_t);

#define csum_set(i, type)						\
({									\
	void *start = ((void *) (i)) + sizeof(uint64_t);		\
	void *end = bset_bkey_last(i);					\
									\
	bch_checksum(type, start, end - start);				\
})

#endif
