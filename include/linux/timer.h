#ifndef __TOOLS_LINUX_TIMER_H
#define __TOOLS_LINUX_TIMER_H

#include <string.h>
#include <linux/types.h>

struct timer_list {
	unsigned long		expires;
	void			(*function)(unsigned long);
	unsigned long		data;
	bool			pending;
};

static inline void init_timer(struct timer_list *timer)
{
	memset(timer, 0, sizeof(*timer));
}

#define __init_timer(_timer, _flags)	init_timer(_timer)

#define __setup_timer(_timer, _fn, _data, _flags)			\
	do {								\
		__init_timer((_timer), (_flags));			\
		(_timer)->function = (_fn);				\
		(_timer)->data = (_data);				\
	} while (0)

#define setup_timer(timer, fn, data)					\
	__setup_timer((timer), (fn), (data), 0)

static inline int timer_pending(const struct timer_list *timer)
{
	return timer->pending;
}

int del_timer(struct timer_list * timer);
int del_timer_sync(struct timer_list *timer);

int mod_timer(struct timer_list *timer, unsigned long expires);
//extern int mod_timer_pending(struct timer_list *timer, unsigned long expires);

static inline void add_timer(struct timer_list *timer)
{
	BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}

void flush_timers(void);

#endif /* __TOOLS_LINUX_TIMER_H */
