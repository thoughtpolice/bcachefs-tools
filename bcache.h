/*
 * Author: Kent Overstreet <kmo@daterainc.com>
 *
 * GPLv2
 */

#ifndef _BCACHE_H
#define _BCACHE_H

#include <linux/bcache.h>
#include <dirent.h>

typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef __s8	s8;
typedef __s16	s16;
typedef __s32	s32;
typedef __s64	s64;

#define SB_START		(SB_SECTOR * 512)
#define MAX_PATH		256
#define MAX_DEVS		MAX_CACHES_PER_SET

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define __bkey_idx(_set, _offset)				\
	((_set)->_data + (_offset))

#define bkey_idx(_set, _offset)					\
	((typeof(&(_set)->start[0])) __bkey_idx((_set), (_offset)))

#define bkey_next(_k)						\
	((typeof(_k)) __bkey_idx(_k, (_k)->u64s))

#define __bset_bkey_last(_set)					\
	 __bkey_idx((_set), (_set)->u64s)

#define bset_bkey_last(_set)					\
	 bkey_idx((_set), (_set)->u64s)

extern const char * const cache_state[];
extern const char * const replacement_policies[];
extern const char * const csum_types[];
extern const char * const bdev_cache_mode[];
extern const char * const bdev_state[];
extern const char * const set_attr[];
extern const char * const cache_attrs[];
extern const char * const internal_attrs[];

ssize_t read_string_list(const char *, const char * const[]);
ssize_t read_string_list_or_die(const char *, const char * const[],
				const char *);
void print_string_list(const char * const[], size_t);

uint64_t bch_checksum(unsigned, const void *, size_t);

uint64_t getblocks(int);
uint64_t hatoi(const char *);
unsigned hatoi_validate(const char *, const char *);
void write_backingdev_sb(int, unsigned, unsigned *, unsigned, uint64_t,
			 const char *, uuid_le, uuid_le);
int dev_open(const char *, bool);
void write_cache_sbs(int *, struct cache_sb *, unsigned, unsigned *, int);
void next_cache_device(struct cache_sb *, unsigned, int, unsigned, bool);
unsigned get_blocksize(const char *);
long strtoul_or_die(const char *, size_t, const char *);

void show_super_backingdev(struct cache_sb *, bool);
void show_super_cache(struct cache_sb *, bool);

enum sysfs_attr {SET_ATTR, CACHE_ATTR, INTERNAL_ATTR};

enum sysfs_attr sysfs_attr_type(const char *attr);
void sysfs_attr_list();

struct cache_sb *query_dev(char *, bool, bool, bool, char *dev_uuid);
char *list_cachesets(char *, bool);
char *parse_array_to_list(char *const *);
char *register_bcache(char *const *);
char *unregister_bcache(char *const *);
char *probe(char *, int);
char *read_stat_dir(DIR *, char *, char *, char *);
char *find_matching_uuid(char *, char *, const char*);
char *add_devices(char *const *);
char *remove_device(const char *, bool);
char *bcache_get_capacity(const char *, const char *, bool);
char *dev_name(const char *);
char *device_set_failed(const char *dev_uuid);

#define csum_set(i, type)						\
({									\
	void *start = ((void *) (i)) + sizeof(uint64_t);		\
	void *end = __bset_bkey_last(i);				\
									\
	bch_checksum(type, start, end - start);				\
})

#endif
