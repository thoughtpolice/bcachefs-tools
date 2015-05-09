#ifndef _BCACHEADM_QUERY_H
#define _BCACHEADM_QUERY_H

extern NihOption opts_list[];
int cmd_list(NihCommand *, char * const *);

extern NihOption opts_query[];
int cmd_query(NihCommand *, char * const *);

extern NihOption opts_status[];
int cmd_status(NihCommand *, char * const *);

#endif /* _BCACHEADM_QUERY_H */
