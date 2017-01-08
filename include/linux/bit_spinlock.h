#ifndef __LINUX_BIT_SPINLOCK_H
#define __LINUX_BIT_SPINLOCK_H

#include <linux/kernel.h>
#include <linux/preempt.h>
#include <linux/atomic.h>
#include <linux/bug.h>

static inline void bit_spin_lock(int bitnum, unsigned long *addr)
{
	while (unlikely(test_and_set_bit_lock(bitnum, addr))) {
		do {
			cpu_relax();
		} while (test_bit(bitnum, addr));
	}
}

static inline int bit_spin_trylock(int bitnum, unsigned long *addr)
{
	return !test_and_set_bit_lock(bitnum, addr);
}

static inline void bit_spin_unlock(int bitnum, unsigned long *addr)
{
	BUG_ON(!test_bit(bitnum, addr));

	clear_bit_unlock(bitnum, addr);
}

static inline void __bit_spin_unlock(int bitnum, unsigned long *addr)
{
	bit_spin_unlock(bitnum, addr);
}

static inline int bit_spin_is_locked(int bitnum, unsigned long *addr)
{
	return test_bit(bitnum, addr);
}

#endif /* __LINUX_BIT_SPINLOCK_H */

