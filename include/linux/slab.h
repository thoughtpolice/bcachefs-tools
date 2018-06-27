#ifndef __TOOLS_LINUX_SLAB_H
#define __TOOLS_LINUX_SLAB_H

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <linux/kernel.h>
#include <linux/page.h>
#include <linux/shrinker.h>
#include <linux/types.h>

#define ARCH_KMALLOC_MINALIGN		16
#define KMALLOC_MAX_SIZE		SIZE_MAX

static inline void *kmalloc(size_t size, gfp_t flags)
{
	void *p;

	run_shrinkers();

	p = malloc(size);
	if (p && (flags & __GFP_ZERO))
		memset(p, 0, size);

	return p;
}

static inline void *krealloc(void *old, size_t size, gfp_t flags)
{
	void *new;

	run_shrinkers();

	new = malloc(size);
	if (!new)
		return NULL;

	if (flags & __GFP_ZERO)
		memset(new, 0, size);

	memcpy(new, old,
	       min(malloc_usable_size(old),
		   malloc_usable_size(new)));
	free(old);

	return new;
}

#define kzalloc(size, flags)		kmalloc(size, flags|__GFP_ZERO)
#define kmalloc_array(n, size, flags)					\
	((size) != 0 && (n) > SIZE_MAX / (size)				\
	 ? NULL : kmalloc(n * size, flags))

#define kcalloc(n, size, flags)		kmalloc_array(n, size, flags|__GFP_ZERO)

#define kfree(p)			free(p)
#define kvfree(p)			free(p)
#define kzfree(p)			free(p)

static inline struct page *alloc_pages(gfp_t flags, unsigned int order)
{
	size_t size = PAGE_SIZE << order;
	void *p;

	run_shrinkers();

	p = aligned_alloc(PAGE_SIZE, size);
	if (p && (flags & __GFP_ZERO))
		memset(p, 0, size);

	return p;
}

#define alloc_page(gfp)			alloc_pages(gfp, 0)

#define __get_free_pages(gfp, order)	((unsigned long) alloc_pages(gfp, order))
#define __get_free_page(gfp)		__get_free_pages(gfp, 0)

#define __free_pages(page, order)			\
do {							\
	(void) order;					\
	free(page);					\
} while (0)

#define free_pages(addr, order)				\
do {							\
	(void) order;					\
	free((void *) (addr));				\
} while (0)

#define __free_page(page) __free_pages((page), 0)
#define free_page(addr) free_pages((addr), 0)

#define VM_IOREMAP		0x00000001	/* ioremap() and friends */
#define VM_ALLOC		0x00000002	/* vmalloc() */
#define VM_MAP			0x00000004	/* vmap()ed pages */
#define VM_USERMAP		0x00000008	/* suitable for remap_vmalloc_range */
#define VM_UNINITIALIZED	0x00000020	/* vm_struct is not fully initialized */
#define VM_NO_GUARD		0x00000040      /* don't add guard page */
#define VM_KASAN		0x00000080      /* has allocated kasan shadow memory */

static inline void vunmap(const void *addr) {}

static inline void *vmap(struct page **pages, unsigned int count,
			 unsigned long flags, unsigned prot)
{
	return NULL;
}

#define is_vmalloc_addr(page)		0

#define vmalloc_to_page(addr)		((struct page *) (addr))

static inline void *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *p;

	p = kmalloc(len, gfp);
	if (p)
		memcpy(p, src, len);
	return p;
}

#endif /* __TOOLS_LINUX_SLAB_H */
