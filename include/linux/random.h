/*
 * include/linux/random.h
 *
 * Include file for the random number generator.
 */
#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/bug.h>

#ifdef __NR_getrandom
static inline int getrandom(void *buf, size_t buflen, unsigned int flags)
{
	 return syscall(SYS_getrandom, buf, buflen, flags);
}
#else
extern int urandom_fd;

static inline int getrandom(void *buf, size_t buflen, unsigned int flags)
{
	return read(urandom_fd, buf, buflen);
}
#endif

static inline void get_random_bytes(void *buf, int nbytes)
{
	BUG_ON(getrandom(buf, nbytes, 0) != nbytes);
}

static inline int get_random_int(void)
{
	int v;

	get_random_bytes(&v, sizeof(v));
	return v;
}

static inline long get_random_long(void)
{
	long v;

	get_random_bytes(&v, sizeof(v));
	return v;
}

#endif /* _LINUX_RANDOM_H */
