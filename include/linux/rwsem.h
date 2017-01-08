#ifndef __TOOLS_LINUX_RWSEM_H
#define __TOOLS_LINUX_RWSEM_H

#include <pthread.h>

struct rw_semaphore {
	pthread_rwlock_t	lock;
};

#define __RWSEM_INITIALIZER(name)				\
	{ .lock = PTHREAD_RWLOCK_INITIALIZER }

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = __RWSEM_INITIALIZER(name)

static inline void init_rwsem(struct rw_semaphore *lock)
{
	pthread_rwlock_init(&lock->lock, NULL);
}

#define down_read(l)		pthread_rwlock_rdlock(&(l)->lock)
#define down_read_trylock(l)	(!pthread_rwlock_tryrdlock(&(l)->lock))
#define up_read(l)		pthread_rwlock_unlock(&(l)->lock)

#define down_write(l)		pthread_rwlock_wrlock(&(l)->lock)
#define up_write(l)		pthread_rwlock_unlock(&(l)->lock)

#endif /* __TOOLS_LINUX_RWSEM_H */
