#ifndef _BCACHE_FS_H
#define _BCACHE_FS_H

extern NihOption opts_fs_show[];
int cmd_fs_show(NihCommand *, char * const *);

extern NihOption opts_fs_set[];
int cmd_fs_set(NihCommand *, char * const *);

#endif /* _BCACHE_FS_H */
