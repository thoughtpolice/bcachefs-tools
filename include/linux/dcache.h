#ifndef __LINUX_DCACHE_H
#define __LINUX_DCACHE_H

struct super_block;
struct inode;

struct dentry {
	struct super_block *d_sb;
	struct inode *d_inode;
};

#endif	/* __LINUX_DCACHE_H */
