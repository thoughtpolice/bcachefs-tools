
#include <linux/futex.h>
#include <string.h>
#include <sys/mman.h>

/* hack for mips: */
#define CONFIG_RCU_HAVE_FUTEX 1
#include <urcu/futex.h>

#include <linux/math64.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timer.h>

__thread struct task_struct *current;

void __put_task_struct(struct task_struct *t)
{
	pthread_join(t->thread, NULL);
	free(t);
}

/* returns true if process was woken up, false if it was already running */
int wake_up_process(struct task_struct *p)
{
	int ret = p->state != TASK_RUNNING;

	p->state = TASK_RUNNING;
	futex(&p->state, FUTEX_WAKE|FUTEX_PRIVATE_FLAG,
	      INT_MAX, NULL, NULL, 0);
	return ret;
}

void schedule(void)
{
	int v;

	rcu_quiescent_state();

	while ((v = current->state) != TASK_RUNNING)
		futex(&current->state, FUTEX_WAIT|FUTEX_PRIVATE_FLAG,
		      v, NULL, NULL, 0);
}

struct process_timer {
	struct timer_list timer;
	struct task_struct *task;
};

static void process_timeout(struct timer_list *t)
{
	struct process_timer *timeout =
		container_of(t, struct process_timer, timer);

	wake_up_process(timeout->task);
}

long schedule_timeout(long timeout)
{
	struct process_timer timer;
	unsigned long expire;

	switch (timeout)
	{
	case MAX_SCHEDULE_TIMEOUT:
		/*
		 * These two special cases are useful to be comfortable
		 * in the caller. Nothing more. We could take
		 * MAX_SCHEDULE_TIMEOUT from one of the negative value
		 * but I' d like to return a valid offset (>=0) to allow
		 * the caller to do everything it want with the retval.
		 */
		schedule();
		goto out;
	default:
		/*
		 * Another bit of PARANOID. Note that the retval will be
		 * 0 since no piece of kernel is supposed to do a check
		 * for a negative retval of schedule_timeout() (since it
		 * should never happens anyway). You just have the printk()
		 * that will tell you if something is gone wrong and where.
		 */
		if (timeout < 0) {
			printk(KERN_ERR "schedule_timeout: wrong timeout "
				"value %lx\n", timeout);
			current->state = TASK_RUNNING;
			goto out;
		}
	}

	expire = timeout + jiffies;

	timer.task = current;
	timer_setup_on_stack(&timer.timer, process_timeout, 0);
	mod_timer(&timer.timer, expire);
	schedule();
	del_timer_sync(&timer.timer);

	timeout = expire - jiffies;
out:
	return timeout < 0 ? 0 : timeout;
}

unsigned long __msecs_to_jiffies(const unsigned int m)
{
	/*
	 * Negative value, means infinite timeout:
	 */
	if ((int)m < 0)
		return MAX_JIFFY_OFFSET;
	return _msecs_to_jiffies(m);
}

u64 nsecs_to_jiffies64(u64 n)
{
#if (NSEC_PER_SEC % HZ) == 0
	/* Common case, HZ = 100, 128, 200, 250, 256, 500, 512, 1000 etc. */
	return div_u64(n, NSEC_PER_SEC / HZ);
#elif (HZ % 512) == 0
	/* overflow after 292 years if HZ = 1024 */
	return div_u64(n * HZ / 512, NSEC_PER_SEC / 512);
#else
	/*
	 * Generic case - optimized for cases where HZ is a multiple of 3.
	 * overflow after 64.99 years, exact for HZ = 60, 72, 90, 120 etc.
	 */
	return div_u64(n * 9, (9ull * NSEC_PER_SEC + HZ / 2) / HZ);
#endif
}

unsigned long nsecs_to_jiffies(u64 n)
{
	return (unsigned long)nsecs_to_jiffies64(n);
}

unsigned int jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
# if BITS_PER_LONG == 32
	return (HZ_TO_MSEC_MUL32 * j) >> HZ_TO_MSEC_SHR32;
# else
	return (j * HZ_TO_MSEC_NUM) / HZ_TO_MSEC_DEN;
# endif
#endif
}

unsigned int jiffies_to_usecs(const unsigned long j)
{
	/*
	 * Hz usually doesn't go much further MSEC_PER_SEC.
	 * jiffies_to_usecs() and usecs_to_jiffies() depend on that.
	 */
	BUILD_BUG_ON(HZ > USEC_PER_SEC);

#if !(USEC_PER_SEC % HZ)
	return (USEC_PER_SEC / HZ) * j;
#else
# if BITS_PER_LONG == 32
	return (HZ_TO_USEC_MUL32 * j) >> HZ_TO_USEC_SHR32;
# else
	return (j * HZ_TO_USEC_NUM) / HZ_TO_USEC_DEN;
# endif
#endif
}

__attribute__((constructor(101)))
static void sched_init(void)
{
	struct task_struct *p = malloc(sizeof(*p));

	mlockall(MCL_CURRENT|MCL_FUTURE);

	memset(p, 0, sizeof(*p));

	p->state	= TASK_RUNNING;
	atomic_set(&p->usage, 1);
	init_completion(&p->exited);

	current = p;

	rcu_init();
	rcu_register_thread();
}

#ifndef __NR_getrandom
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
int urandom_fd;

__attribute__((constructor(101)))
static void rand_init(void)
{
	urandom_fd = open("/dev/urandom", O_RDONLY);
	BUG_ON(urandom_fd < 0);
}
#endif
