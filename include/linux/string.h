#ifndef _TOOLS_LINUX_STRING_H_
#define _TOOLS_LINUX_STRING_H_

#include <stdlib.h>
#include <string.h>
#include <linux/types.h>	/* for size_t */

extern size_t strlcpy(char *dest, const char *src, size_t size);
extern char *skip_spaces(const char *);
extern char *strim(char *);
extern void memzero_explicit(void *, size_t);
int match_string(const char * const *, size_t, const char *);

#define kstrndup(s, n, gfp)		strndup(s, n)

#endif /* _LINUX_STRING_H_ */
