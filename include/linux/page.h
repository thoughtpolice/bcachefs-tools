#ifndef _LINUX_PAGE_H
#define _LINUX_PAGE_H

#include <sys/user.h>

struct page;

#define virt_to_page(kaddr)		((struct page *) (kaddr))
#define page_address(kaddr)		((void *) (kaddr))

#define kmap_atomic(page)		page_address(page)
#define kunmap_atomic(addr)		do {} while (0)

static const char zero_page[PAGE_SIZE];

#define ZERO_PAGE(o)			((struct page *) &zero_page[0])

#endif /* _LINUX_PAGE_H */
