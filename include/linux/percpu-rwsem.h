
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PERCPU_RWSEM_H
#define _LINUX_PERCPU_RWSEM_H

#include <pthread.h>
#include <linux/preempt.h>

struct percpu_rw_semaphore {
	pthread_rwlock_t	lock;
};

#define DEFINE_STATIC_PERCPU_RWSEM(name)				\
static DEFINE_PER_CPU(unsigned int, __percpu_rwsem_rc_##name);		\
static struct percpu_rw_semaphore name = {				\
	.rss = __RCU_SYNC_INITIALIZER(name.rss, RCU_SCHED_SYNC),	\
	.read_count = &__percpu_rwsem_rc_##name,			\
	.rw_sem = __RWSEM_INITIALIZER(name.rw_sem),			\
	.writer = __RCUWAIT_INITIALIZER(name.writer),			\
}

extern int __percpu_down_read(struct percpu_rw_semaphore *, int);
extern void __percpu_up_read(struct percpu_rw_semaphore *);

static inline void percpu_down_read_preempt_disable(struct percpu_rw_semaphore *sem)
{
	pthread_rwlock_rdlock(&sem->lock);
	preempt_disable();
}

static inline void percpu_down_read(struct percpu_rw_semaphore *sem)
{
	pthread_rwlock_rdlock(&sem->lock);
}

static inline int percpu_down_read_trylock(struct percpu_rw_semaphore *sem)
{
	return !pthread_rwlock_tryrdlock(&sem->lock);
}

static inline void percpu_up_read_preempt_enable(struct percpu_rw_semaphore *sem)
{
	preempt_enable();
	pthread_rwlock_unlock(&sem->lock);
}

static inline void percpu_up_read(struct percpu_rw_semaphore *sem)
{
	pthread_rwlock_unlock(&sem->lock);
}

static inline void percpu_down_write(struct percpu_rw_semaphore *sem)
{
	pthread_rwlock_wrlock(&sem->lock);
}

static inline void percpu_up_write(struct percpu_rw_semaphore *sem)
{
	pthread_rwlock_unlock(&sem->lock);
}

static inline void percpu_free_rwsem(struct percpu_rw_semaphore *sem) {}

static inline int percpu_init_rwsem(struct percpu_rw_semaphore *sem)
{
	pthread_rwlock_init(&sem->lock, NULL);
	return 0;
}

#define percpu_rwsem_assert_held(sem)		do {} while (0)

#endif
