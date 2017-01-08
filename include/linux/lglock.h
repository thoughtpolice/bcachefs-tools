#ifndef __TOOLS_LINUX_LGLOCK_H
#define __TOOLS_LINUX_LGLOCK_H

#include <pthread.h>

struct lglock {
	pthread_mutex_t lock;
};

#define lg_lock_free(l)		do {} while (0)
#define lg_lock_init(l)		pthread_mutex_init(&(l)->lock, NULL)

#define lg_local_lock(l)	pthread_mutex_lock(&(l)->lock)
#define lg_local_unlock(l)	pthread_mutex_unlock(&(l)->lock)
#define lg_global_lock(l)	pthread_mutex_lock(&(l)->lock)
#define lg_global_unlock(l)	pthread_mutex_unlock(&(l)->lock)

#endif /* __TOOLS_LINUX_LGLOCK_H */
