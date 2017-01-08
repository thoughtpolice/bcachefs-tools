/*
 * memory buffer pool support
 */
#ifndef _LINUX_MEMPOOL_H
#define _LINUX_MEMPOOL_H

#include <linux/compiler.h>
#include <linux/bug.h>
#include <linux/slab.h>

struct kmem_cache;

typedef struct mempool_s {
	size_t		elem_size;
} mempool_t;

extern int mempool_resize(mempool_t *pool, int new_min_nr);

static inline void mempool_free(void *element, mempool_t *pool)
{
	free(element);
}

static inline void *mempool_alloc(mempool_t *pool, gfp_t gfp_mask) __malloc
{
	BUG_ON(!pool->elem_size);
	return kmalloc(pool->elem_size, gfp_mask);
}

static inline void mempool_exit(mempool_t *pool) {}

static inline void mempool_destroy(mempool_t *pool)
{
	free(pool);
}

static inline int
mempool_init_slab_pool(mempool_t *pool, int min_nr, struct kmem_cache *kc)
{
	pool->elem_size = 0;
	return 0;
}

static inline mempool_t *
mempool_create_slab_pool(int min_nr, struct kmem_cache *kc)
{
	mempool_t *pool = malloc(sizeof(*pool));
	pool->elem_size = 0;
	return pool;
}

static inline int mempool_init_kmalloc_pool(mempool_t *pool, int min_nr, size_t size)
{
	pool->elem_size = size;
	return 0;
}

static inline mempool_t *mempool_create_kmalloc_pool(int min_nr, size_t size)
{
	mempool_t *pool = malloc(sizeof(*pool));
	pool->elem_size = size;
	return pool;
}

static inline int mempool_init_page_pool(mempool_t *pool, int min_nr, int order)
{
	pool->elem_size = PAGE_SIZE << order;
	return 0;
}

static inline mempool_t *mempool_create_page_pool(int min_nr, int order)
{
	mempool_t *pool = malloc(sizeof(*pool));
	pool->elem_size = PAGE_SIZE << order;
	return pool;
}

#endif /* _LINUX_MEMPOOL_H */
