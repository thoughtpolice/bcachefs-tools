#ifndef _TOOLS_LINUX_TYPES_H_
#define _TOOLS_LINUX_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/types.h>

#define __SANE_USERSPACE_TYPES__	/* For PPC64, to get LL64 types */
#include <asm/types.h>

#define BITS_PER_LONG	__BITS_PER_LONG

struct page;
struct kmem_cache;

typedef unsigned long		pgoff_t;

typedef unsigned short		umode_t;

typedef unsigned gfp_t;

#define GFP_KERNEL	0
#define GFP_ATOMIC	0
#define GFP_NOFS	0
#define GFP_NOIO	0
#define GFP_NOWAIT	0
#define __GFP_IO	0
#define __GFP_NOWARN	0
#define __GFP_NORETRY	0
#define __GFP_ZERO	1

#define PAGE_ALLOC_COSTLY_ORDER	6

typedef __u64 u64;
typedef __s64 s64;
typedef __u32 u32;
typedef __s32 s32;
typedef __u16 u16;
typedef __s16 s16;
typedef __u8  u8;
typedef __s8  s8;

#ifdef __CHECKER__
#define __bitwise__ __attribute__((bitwise))
#else
#define __bitwise__
#endif
#ifdef __CHECK_ENDIAN__
#define __bitwise __bitwise__
#else
#define __bitwise
#endif

#define __force
#define __user
#define __must_check
#define __cold

typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;

#ifndef __aligned_u64
# define __aligned_u64 __u64 __attribute__((aligned(8)))
#endif

typedef u64 sector_t;

#endif /* _TOOLS_LINUX_TYPES_H_ */
