/*
 * Resizable, Scalable, Concurrent Hash Table
 *
 * Copyright (c) 2015 Herbert Xu <herbert@gondor.apana.org.au>
 * Copyright (c) 2014-2015 Thomas Graf <tgraf@suug.ch>
 * Copyright (c) 2008-2014 Patrick McHardy <kaber@trash.net>
 *
 * Code partially derived from nft_hash
 * Rewritten with rehash code from br_multicast plus single list
 * pointer as suggested by Josh Triplett
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_RHASHTABLE_H
#define _LINUX_RHASHTABLE_H

#include <linux/atomic.h>
#include <linux/cache.h>
#include <linux/compiler.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/jhash.h>
#include <linux/list_nulls.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>

#define RHT_BASE_BITS		4
#define RHT_HASH_BITS		27
#define RHT_BASE_SHIFT		RHT_HASH_BITS
#define RHT_HASH_RESERVED_SPACE	(RHT_BASE_BITS + 1)

struct rhash_head {
	struct rhash_head __rcu		*next;
};

struct bucket_table {
	unsigned int		size;
	unsigned int		rehash;
	u32			hash_rnd;
	unsigned int		locks_mask;
	spinlock_t		*locks;
	struct list_head	walkers;
	struct rcu_head		rcu;

	struct bucket_table __rcu *future_tbl;

	struct rhash_head __rcu	*buckets[] ____cacheline_aligned_in_smp;
};

struct rhashtable_compare_arg {
	struct rhashtable *ht;
	const void *key;
};

typedef u32 (*rht_hashfn_t)(const void *data, u32 len, u32 seed);
typedef u32 (*rht_obj_hashfn_t)(const void *data, u32 len, u32 seed);
typedef int (*rht_obj_cmpfn_t)(struct rhashtable_compare_arg *arg,
			       const void *obj);

struct rhashtable_params {
	size_t			nelem_hint;
	size_t			key_len;
	size_t			key_offset;
	size_t			head_offset;
	unsigned int		insecure_max_entries;
	unsigned int		max_size;
	unsigned int		min_size;
	u32			nulls_base;
	bool			insecure_elasticity;
	bool			automatic_shrinking;
	size_t			locks_mul;
	rht_hashfn_t		hashfn;
	rht_obj_hashfn_t	obj_hashfn;
	rht_obj_cmpfn_t		obj_cmpfn;
};

struct rhashtable {
	struct bucket_table __rcu	*tbl;
	atomic_t			nelems;
	unsigned int			key_len;
	unsigned int			elasticity;
	struct rhashtable_params	p;
	struct work_struct		run_work;
	struct mutex                    mutex;
	spinlock_t			lock;
};

struct rhashtable_walker {
	struct list_head list;
	struct bucket_table *tbl;
};

static inline unsigned long rht_marker(const struct rhashtable *ht, u32 hash)
{
	return NULLS_MARKER(ht->p.nulls_base + hash);
}

#define INIT_RHT_NULLS_HEAD(ptr, ht, hash) \
	((ptr) = (typeof(ptr)) rht_marker(ht, hash))

static inline bool rht_is_a_nulls(const struct rhash_head *ptr)
{
	return ((unsigned long) ptr & 1);
}

static inline unsigned long rht_get_nulls_value(const struct rhash_head *ptr)
{
	return ((unsigned long) ptr) >> 1;
}

static inline void *rht_obj(const struct rhashtable *ht,
			    const struct rhash_head *he)
{
	return (char *)he - ht->p.head_offset;
}

static inline unsigned int rht_bucket_index(const struct bucket_table *tbl,
					    unsigned int hash)
{
	return (hash >> RHT_HASH_RESERVED_SPACE) & (tbl->size - 1);
}

static inline unsigned int rht_key_hashfn(
	struct rhashtable *ht, const struct bucket_table *tbl,
	const void *key, const struct rhashtable_params params)
{
	unsigned int hash;

	/* params must be equal to ht->p if it isn't constant. */
	if (!__builtin_constant_p(params.key_len))
		hash = ht->p.hashfn(key, ht->key_len, tbl->hash_rnd);
	else if (params.key_len) {
		unsigned int key_len = params.key_len;

		if (params.hashfn)
			hash = params.hashfn(key, key_len, tbl->hash_rnd);
		else if (key_len & (sizeof(u32) - 1))
			hash = jhash(key, key_len, tbl->hash_rnd);
		else
			hash = jhash2(key, key_len / sizeof(u32),
				      tbl->hash_rnd);
	} else {
		unsigned int key_len = ht->p.key_len;

		if (params.hashfn)
			hash = params.hashfn(key, key_len, tbl->hash_rnd);
		else
			hash = jhash(key, key_len, tbl->hash_rnd);
	}

	return rht_bucket_index(tbl, hash);
}

static inline unsigned int rht_head_hashfn(
	struct rhashtable *ht, const struct bucket_table *tbl,
	const struct rhash_head *he, const struct rhashtable_params params)
{
	const char *ptr = rht_obj(ht, he);

	return likely(params.obj_hashfn) ?
	       rht_bucket_index(tbl, params.obj_hashfn(ptr, params.key_len ?:
							    ht->p.key_len,
						       tbl->hash_rnd)) :
	       rht_key_hashfn(ht, tbl, ptr + params.key_offset, params);
}

static inline bool rht_grow_above_75(const struct rhashtable *ht,
				     const struct bucket_table *tbl)
{
	/* Expand table when exceeding 75% load */
	return atomic_read(&ht->nelems) > (tbl->size / 4 * 3) &&
	       (!ht->p.max_size || tbl->size < ht->p.max_size);
}

static inline bool rht_shrink_below_30(const struct rhashtable *ht,
				       const struct bucket_table *tbl)
{
	/* Shrink table beneath 30% load */
	return atomic_read(&ht->nelems) < (tbl->size * 3 / 10) &&
	       tbl->size > ht->p.min_size;
}

static inline bool rht_grow_above_100(const struct rhashtable *ht,
				      const struct bucket_table *tbl)
{
	return atomic_read(&ht->nelems) > tbl->size &&
		(!ht->p.max_size || tbl->size < ht->p.max_size);
}

static inline bool rht_grow_above_max(const struct rhashtable *ht,
				      const struct bucket_table *tbl)
{
	return ht->p.insecure_max_entries &&
	       atomic_read(&ht->nelems) >= ht->p.insecure_max_entries;
}

static inline spinlock_t *rht_bucket_lock(const struct bucket_table *tbl,
					  unsigned int hash)
{
	return &tbl->locks[hash & tbl->locks_mask];
}

int rhashtable_insert_rehash(struct rhashtable *, struct bucket_table *);
struct bucket_table *rhashtable_insert_slow(struct rhashtable *,
					    const void *,
					    struct rhash_head *,
					    struct bucket_table *);

int rhashtable_init(struct rhashtable *, const struct rhashtable_params *);
void rhashtable_destroy(struct rhashtable *);

#define rht_dereference(p, ht)			rcu_dereference(p)
#define rht_dereference_rcu(p, ht)		rcu_dereference(p)
#define rht_dereference_bucket(p, tbl, hash)	rcu_dereference(p)
#define rht_dereference_bucket_rcu(p, tbl, hash) rcu_dereference(p)

#define rht_entry(tpos, pos, member) \
	({ tpos = container_of(pos, typeof(*tpos), member); 1; })

#define rht_for_each_continue(pos, head, tbl, hash) \
	for (pos = rht_dereference_bucket(head, tbl, hash); \
	     !rht_is_a_nulls(pos); \
	     pos = rht_dereference_bucket((pos)->next, tbl, hash))

#define rht_for_each(pos, tbl, hash) \
	rht_for_each_continue(pos, (tbl)->buckets[hash], tbl, hash)

#define rht_for_each_rcu_continue(pos, head, tbl, hash)			\
	for (({barrier(); }),						\
	     pos = rht_dereference_bucket_rcu(head, tbl, hash);		\
	     !rht_is_a_nulls(pos);					\
	     pos = rcu_dereference_raw(pos->next))

#define rht_for_each_rcu(pos, tbl, hash)				\
	rht_for_each_rcu_continue(pos, (tbl)->buckets[hash], tbl, hash)

#define rht_for_each_entry_rcu_continue(tpos, pos, head, tbl, hash, member) \
	for (({barrier(); }),						    \
	     pos = rht_dereference_bucket_rcu(head, tbl, hash);		    \
	     (!rht_is_a_nulls(pos)) && rht_entry(tpos, pos, member);	    \
	     pos = rht_dereference_bucket_rcu(pos->next, tbl, hash))

#define rht_for_each_entry_rcu(tpos, pos, tbl, hash, member)		\
	rht_for_each_entry_rcu_continue(tpos, pos, (tbl)->buckets[hash],\
					tbl, hash, member)

static inline int rhashtable_compare(struct rhashtable_compare_arg *arg,
				     const void *obj)
{
	struct rhashtable *ht = arg->ht;
	const char *ptr = obj;

	return memcmp(ptr + ht->p.key_offset, arg->key, ht->p.key_len);
}

static inline void *rhashtable_lookup_fast(
	struct rhashtable *ht, const void *key,
	const struct rhashtable_params params)
{
	struct rhashtable_compare_arg arg = {
		.ht = ht,
		.key = key,
	};
	const struct bucket_table *tbl;
	struct rhash_head *he;
	unsigned int hash;

	rcu_read_lock();

	tbl = rht_dereference_rcu(ht->tbl, ht);
restart:
	hash = rht_key_hashfn(ht, tbl, key, params);
	rht_for_each_rcu(he, tbl, hash) {
		if (params.obj_cmpfn ?
		    params.obj_cmpfn(&arg, rht_obj(ht, he)) :
		    rhashtable_compare(&arg, rht_obj(ht, he)))
			continue;
		rcu_read_unlock();
		return rht_obj(ht, he);
	}

	/* Ensure we see any new tables. */
	smp_rmb();

	tbl = rht_dereference_rcu(tbl->future_tbl, ht);
	if (unlikely(tbl))
		goto restart;
	rcu_read_unlock();

	return NULL;
}

static inline int __rhashtable_insert_fast(
	struct rhashtable *ht, const void *key, struct rhash_head *obj,
	const struct rhashtable_params params)
{
	struct rhashtable_compare_arg arg = {
		.ht = ht,
		.key = key,
	};
	struct bucket_table *tbl, *new_tbl;
	struct rhash_head *head;
	spinlock_t *lock;
	unsigned int elasticity;
	unsigned int hash;
	int err;

restart:
	rcu_read_lock();

	tbl = rht_dereference_rcu(ht->tbl, ht);

	/* All insertions must grab the oldest table containing
	 * the hashed bucket that is yet to be rehashed.
	 */
	for (;;) {
		hash = rht_head_hashfn(ht, tbl, obj, params);
		lock = rht_bucket_lock(tbl, hash);
		spin_lock_bh(lock);

		if (tbl->rehash <= hash)
			break;

		spin_unlock_bh(lock);
		tbl = rht_dereference_rcu(tbl->future_tbl, ht);
	}

	new_tbl = rht_dereference_rcu(tbl->future_tbl, ht);
	if (unlikely(new_tbl)) {
		tbl = rhashtable_insert_slow(ht, key, obj, new_tbl);
		if (!IS_ERR_OR_NULL(tbl))
			goto slow_path;

		err = PTR_ERR(tbl);
		goto out;
	}

	err = -E2BIG;
	if (unlikely(rht_grow_above_max(ht, tbl)))
		goto out;

	if (unlikely(rht_grow_above_100(ht, tbl))) {
slow_path:
		spin_unlock_bh(lock);
		err = rhashtable_insert_rehash(ht, tbl);
		rcu_read_unlock();
		if (err)
			return err;

		goto restart;
	}

	err = -EEXIST;
	elasticity = ht->elasticity;
	rht_for_each(head, tbl, hash) {
		if (key &&
		    unlikely(!(params.obj_cmpfn ?
			       params.obj_cmpfn(&arg, rht_obj(ht, head)) :
			       rhashtable_compare(&arg, rht_obj(ht, head)))))
			goto out;
		if (!--elasticity)
			goto slow_path;
	}

	err = 0;

	head = rht_dereference_bucket(tbl->buckets[hash], tbl, hash);

	RCU_INIT_POINTER(obj->next, head);

	rcu_assign_pointer(tbl->buckets[hash], obj);

	atomic_inc(&ht->nelems);
	if (rht_grow_above_75(ht, tbl))
		schedule_work(&ht->run_work);

out:
	spin_unlock_bh(lock);
	rcu_read_unlock();

	return err;
}

static inline int rhashtable_lookup_insert_fast(
	struct rhashtable *ht, struct rhash_head *obj,
	const struct rhashtable_params params)
{
	const char *key = rht_obj(ht, obj);

	BUG_ON(ht->p.obj_hashfn);

	return __rhashtable_insert_fast(ht, key + ht->p.key_offset, obj,
					params);
}

static inline int __rhashtable_remove_fast(
	struct rhashtable *ht, struct bucket_table *tbl,
	struct rhash_head *obj, const struct rhashtable_params params)
{
	struct rhash_head __rcu **pprev;
	struct rhash_head *he;
	spinlock_t * lock;
	unsigned int hash;
	int err = -ENOENT;

	hash = rht_head_hashfn(ht, tbl, obj, params);
	lock = rht_bucket_lock(tbl, hash);

	spin_lock_bh(lock);

	pprev = &tbl->buckets[hash];
	rht_for_each(he, tbl, hash) {
		if (he != obj) {
			pprev = &he->next;
			continue;
		}

		rcu_assign_pointer(*pprev, obj->next);
		err = 0;
		break;
	}

	spin_unlock_bh(lock);

	return err;
}

static inline int rhashtable_remove_fast(
	struct rhashtable *ht, struct rhash_head *obj,
	const struct rhashtable_params params)
{
	struct bucket_table *tbl;
	int err;

	rcu_read_lock();

	tbl = rht_dereference_rcu(ht->tbl, ht);

	/* Because we have already taken (and released) the bucket
	 * lock in old_tbl, if we find that future_tbl is not yet
	 * visible then that guarantees the entry to still be in
	 * the old tbl if it exists.
	 */
	while ((err = __rhashtable_remove_fast(ht, tbl, obj, params)) &&
	       (tbl = rht_dereference_rcu(tbl->future_tbl, ht)))
		;

	if (err)
		goto out;

	atomic_dec(&ht->nelems);
	if (unlikely(ht->p.automatic_shrinking &&
		     rht_shrink_below_30(ht, tbl)))
		schedule_work(&ht->run_work);

out:
	rcu_read_unlock();

	return err;
}

#endif /* _LINUX_RHASHTABLE_H */
