#ifndef __LINUX_DCACHE_H
#define __LINUX_DCACHE_H

struct super_block;
struct inode;

/* The hash is always the low bits of hash_len */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
 #define HASH_LEN_DECLARE u32 hash; u32 len
#else
 #define HASH_LEN_DECLARE u32 len; u32 hash
#endif

struct qstr {
	union {
		struct {
			HASH_LEN_DECLARE;
		};
		u64 hash_len;
	};
	const unsigned char *name;
};

#define QSTR_INIT(n,l) { { { .len = l } }, .name = n }

struct dentry {
	struct super_block *d_sb;
	struct inode *d_inode;
};

#endif	/* __LINUX_DCACHE_H */
