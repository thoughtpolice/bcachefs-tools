#include <stdlib.h>

unsigned long long simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return strtoull(cp, endp, base);
}

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return strtoul(cp, endp, base);
}

long long simple_strtoll(const char *cp, char **endp, unsigned int base)
{
	return strtoll(cp, endp, base);
}

long simple_strtol(const char *cp, char **endp, unsigned int base)
{
	return strtol(cp, endp, base);
}
