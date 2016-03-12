#ifndef _BCACHE_RUN_H
#define _BCACHE_RUN_H

extern NihOption opts_run[];
int cmd_run(NihCommand *, char * const *);

extern NihOption opts_stop[];
int cmd_stop(NihCommand *, char * const *);

#endif /* _BCACHE_RUN_H */
