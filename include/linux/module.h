#ifndef _LINUX_MODULE_H
#define _LINUX_MODULE_H

#include <linux/stat.h>
#include <linux/compiler.h>
#include <linux/moduleparam.h>
#include <linux/export.h>

struct module;

#define module_init(initfn)					\
	__attribute__((constructor(109)))			\
	static void __call_##initfn(void) { BUG_ON(initfn()); }

#if 0
#define module_exit(exitfn)					\
	__attribute__((destructor(109)))			\
	static void __call_##exitfn(void) { exitfn(); }
#endif

#define module_exit(exitfn)					\
	__attribute__((unused))					\
	static void __call_##exitfn(void) { exitfn(); }

#define MODULE_INFO(tag, info)
#define MODULE_ALIAS(_alias)
#define MODULE_SOFTDEP(_softdep)
#define MODULE_LICENSE(_license)
#define MODULE_AUTHOR(_author)
#define MODULE_DESCRIPTION(_description)
#define MODULE_VERSION(_version)

static inline void __module_get(struct module *module)
{
}

static inline int try_module_get(struct module *module)
{
	return 1;
}

static inline void module_put(struct module *module)
{
}

#endif /* _LINUX_MODULE_H */
