#ifndef _BCACHE_DEVICE_H
#define _BCACHE_DEVICE_H

extern NihOption opts_device_show[];
int cmd_device_show(NihCommand *, char * const *);

extern NihOption opts_device_add[];
int cmd_device_add(NihCommand *, char * const *);

extern NihOption opts_device_remove[];
int cmd_device_remove(NihCommand *, char * const *);

#endif /* _BCACHE_FORMAT_H */

