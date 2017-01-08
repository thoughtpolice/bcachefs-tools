#ifndef __TOOLS_LINUX_VMALLOC_H
#define __TOOLS_LINUX_VMALLOC_H

#define vmalloc(size)		malloc(size)
#define __vmalloc(size, flags, prot)	malloc(size)
#define vfree(p)		free(p)

#endif /* __TOOLS_LINUX_VMALLOC_H */
