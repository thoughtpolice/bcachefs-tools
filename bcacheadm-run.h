#ifndef _BCACHEADM_RUN_H
#define _BCACHEADM_RUN_H

extern NihOption opts_run[];
int cmd_run(NihCommand *, char * const *);

extern NihOption opts_stop[];
int cmd_stop(NihCommand *, char * const *);

extern NihOption opts_add[];
int cmd_add(NihCommand *, char * const *);

extern NihOption opts_readd[];
int cmd_readd(NihCommand *, char * const *);

extern NihOption opts_remove[];
int cmd_remove(NihCommand *, char * const *);

extern NihOption opts_fail[];
int cmd_fail(NihCommand *, char * const *);

#endif /* _BCACHEADM_RUN_H */
