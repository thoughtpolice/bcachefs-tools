#ifndef __TOOLS_LINUX_ATOMIC_H
#define __TOOLS_LINUX_ATOMIC_H

#include <linux/compiler.h>
#include <linux/types.h>

#define xchg(p, v)						\
	__atomic_exchange_n(p, v, __ATOMIC_SEQ_CST)

#define xchg_acquire(p, v)					\
	__atomic_exchange_n(p, v, __ATOMIC_ACQUIRE)

#define cmpxchg(p, old, new)					\
({								\
	typeof(*(p)) __old = (old);				\
								\
	__atomic_compare_exchange_n((p), &__old, new, false,	\
				    __ATOMIC_SEQ_CST,		\
				    __ATOMIC_SEQ_CST);		\
	__old;							\
})

#define cmpxchg_acquire(p, old, new)				\
({								\
	typeof(*(p)) __old = (old);				\
								\
	__atomic_compare_exchange_n((p), &__old, new, false,	\
				    __ATOMIC_ACQUIRE,		\
				    __ATOMIC_ACQUIRE);		\
	__old;							\
})

#define smp_mb__before_atomic()	__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define smp_mb__after_atomic()	__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define smp_wmb()		__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define smp_rmb()		__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define smp_mb()		__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define smp_read_barrier_depends()

#define smp_store_mb(var, value)  do { WRITE_ONCE(var, value); smp_mb(); } while (0)

#define smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	smp_mb();							\
	___p1;								\
})

#define smp_store_release(p, v)						\
do {									\
	smp_mb();							\
	WRITE_ONCE(*p, v);						\
} while (0)

typedef struct {
	int		counter;
} atomic_t;

static inline int atomic_read(const atomic_t *v)
{
	return __atomic_load_n(&v->counter, __ATOMIC_RELAXED);
}

static inline void atomic_set(atomic_t *v, int i)
{
	__atomic_store_n(&v->counter, i, __ATOMIC_RELAXED);
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	return __atomic_add_fetch(&v->counter, i, __ATOMIC_RELAXED);
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	return __atomic_sub_fetch(&v->counter, i, __ATOMIC_RELAXED);
}

static inline int atomic_add_negative(int i, atomic_t *v)
{
	return atomic_add_return(i, v) < 0;
}

static inline void atomic_add(int i, atomic_t *v)
{
	atomic_add_return(i, v);
}

static inline void atomic_sub(int i, atomic_t *v)
{
	atomic_sub_return(i, v);
}

static inline void atomic_inc(atomic_t *v)
{
	atomic_add(1, v);
}

static inline void atomic_dec(atomic_t *v)
{
	atomic_sub(1, v);
}

#define atomic_dec_return(v)		atomic_sub_return(1, (v))
#define atomic_inc_return(v)		atomic_add_return(1, (v))

#define atomic_sub_and_test(i, v)	(atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v)		(atomic_dec_return(v) == 0)
#define atomic_inc_and_test(v)		(atomic_inc_return(v) == 0)

#define atomic_xchg(ptr, v)		(xchg(&(ptr)->counter, (v)))
#define atomic_cmpxchg(v, old, new)	(cmpxchg(&((v)->counter), (old), (new)))

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	while (c != u && (old = atomic_cmpxchg(v, c, c + a)) != c)
		c = old;
	return c;
}

#define atomic_inc_not_zero(v)		atomic_add_unless((v), 1, 0)

typedef struct {
	long		counter;
} atomic_long_t;

static inline long atomic_long_read(const atomic_long_t *v)
{
	return __atomic_load_n(&v->counter, __ATOMIC_RELAXED);
}

static inline void atomic_long_set(atomic_long_t *v, long i)
{
	__atomic_store_n(&v->counter, i, __ATOMIC_RELAXED);
}

static inline long atomic_long_add_return(long i, atomic_long_t *v)
{
	return __atomic_add_fetch(&v->counter, i, __ATOMIC_RELAXED);
}

static inline long atomic_long_sub_return(long i, atomic_long_t *v)
{
	return __atomic_sub_fetch(&v->counter, i, __ATOMIC_RELAXED);
}

static inline void atomic_long_add(long i, atomic_long_t *v)
{
	atomic_long_add_return(i, v);
}

static inline void atomic_long_sub(long i, atomic_long_t *v)
{
	atomic_long_sub_return(i, v);
}

static inline void atomic_long_inc(atomic_long_t *v)
{
	atomic_long_add(1, v);
}

static inline void atomic_long_dec(atomic_long_t *v)
{
	atomic_long_sub(1, v);
}

static inline long atomic_long_cmpxchg(atomic_long_t *v, long old, long new)
{
	return cmpxchg(&v->counter, old, new);
}

static inline bool atomic_long_inc_not_zero(atomic_long_t *i)
{
	long old, v = atomic_long_read(i);

	do {
		if (!(old = v))
			return false;
	} while ((v = atomic_long_cmpxchg(i, old, old + 1)) != old);

	return true;
}

#define atomic_long_sub_and_test(i, v)	(atomic_long_sub_return((i), (v)) == 0)

typedef struct {
	u64		counter;
} atomic64_t;

static inline s64 atomic64_read(const atomic64_t *v)
{
	return __atomic_load_n(&v->counter, __ATOMIC_RELAXED);
}

static inline void atomic64_set(atomic64_t *v, s64 i)
{
	__atomic_store_n(&v->counter, i, __ATOMIC_RELAXED);
}

static inline s64 atomic64_add_return(s64 i, atomic64_t *v)
{
	return __atomic_add_fetch(&v->counter, i, __ATOMIC_RELAXED);
}

static inline s64 atomic64_sub_return(s64 i, atomic64_t *v)
{
	return __atomic_sub_fetch(&v->counter, i, __ATOMIC_RELAXED);
}

static inline void atomic64_add(s64 i, atomic64_t *v)
{
	atomic64_add_return(i, v);
}

static inline void atomic64_sub(s64 i, atomic64_t *v)
{
	atomic64_sub_return(i, v);
}

static inline void atomic64_inc(atomic64_t *v)
{
	atomic64_add(1, v);
}

static inline void atomic64_dec(atomic64_t *v)
{
	atomic64_sub(1, v);
}

#define atomic64_dec_return(v)		atomic64_sub_return(1, (v))
#define atomic64_inc_return(v)		atomic64_add_return(1, (v))

static inline s64 atomic64_cmpxchg(atomic64_t *v, s64 old, s64 new)
{
	return cmpxchg(&v->counter, old, new);
}

static inline s64 atomic64_cmpxchg_acquire(atomic64_t *v, s64 old, s64 new)
{
	return cmpxchg_acquire(&v->counter, old, new);
}

static inline s64 atomic64_add_return_release(s64 i, atomic64_t *v)
{
	return __atomic_add_fetch(&v->counter, i, __ATOMIC_RELEASE);
}

#endif /* __TOOLS_LINUX_ATOMIC_H */
