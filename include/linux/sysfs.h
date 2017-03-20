#ifndef _SYSFS_H_
#define _SYSFS_H_

#include <linux/compiler.h>
#include <linux/stringify.h>

struct kobject;

struct attribute {
	const char		*name;
	umode_t			mode;
};

#define __ATTR(_name, _mode, _show, _store) {				\
	.attr = {.name = __stringify(_name), .mode = _mode },		\
	.show	= _show,						\
	.store	= _store,						\
}

struct sysfs_ops {
	ssize_t	(*show)(struct kobject *, struct attribute *, char *);
	ssize_t	(*store)(struct kobject *, struct attribute *, const char *, size_t);
};

static inline int sysfs_create_files(struct kobject *kobj,
				    const struct attribute **attr)
{
	return 0;
}

static inline int sysfs_create_link(struct kobject *kobj,
				    struct kobject *target, const char *name)
{
	return 0;
}

static inline void sysfs_remove_link(struct kobject *kobj, const char *name)
{
}

#endif /* _SYSFS_H_ */
