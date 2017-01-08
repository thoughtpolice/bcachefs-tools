#ifndef __TOOLS_LINUX_RCUPDATE_H
#define __TOOLS_LINUX_RCUPDATE_H

#include <urcu.h>
#include <linux/compiler.h>

#define rcu_dereference_check(p, c)	rcu_dereference(p)
#define rcu_dereference_raw(p)		rcu_dereference(p)
#define rcu_dereference_protected(p, c)	rcu_dereference(p)
#define rcu_access_pointer(p)		READ_ONCE(p)

#define kfree_rcu(ptr, rcu_head)	kfree(ptr) /* XXX */

#define RCU_INIT_POINTER(p, v)		WRITE_ONCE(p, v)

#endif /* __TOOLS_LINUX_RCUPDATE_H */
