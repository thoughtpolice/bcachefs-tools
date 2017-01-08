#ifndef _LINUX_RADIX_TREE_H
#define _LINUX_RADIX_TREE_H

struct radix_tree_root {
};

#define INIT_RADIX_TREE(root, mask)	do {} while (0)

static inline void *radix_tree_lookup(struct radix_tree_root *r, unsigned long i)
{
	return NULL;
}

#endif /* _LINUX_RADIX_TREE_H */
