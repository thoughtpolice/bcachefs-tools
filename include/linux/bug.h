#ifndef __TOOLS_LINUX_BUG_H
#define __TOOLS_LINUX_BUG_H

#include <assert.h>
#include <linux/compiler.h>

#define BUILD_BUG_ON_NOT_POWER_OF_2(n)			\
	BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))
#define BUILD_BUG_ON_ZERO(e)	(sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e)	((void *)sizeof(struct { int:-!!(e); }))

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#define BUG()			do { assert(0); unreachable(); } while (0)
#define BUG_ON(cond)		assert(!(cond))

#define WARN_ON_ONCE(cond)	assert(!(cond))
#define WARN_ONCE(cond, msg)	assert(!(cond))

#define __WARN()		assert(0)
#define __WARN_printf(arg...)	assert(0)
#define WARN(cond, ...)		assert(!(cond))

#define WARN_ON(condition) ({						\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		__WARN();						\
	unlikely(__ret_warn_on);					\
})

#endif /* __TOOLS_LINUX_BUG_H */
