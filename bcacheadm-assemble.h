#ifndef _BCACHEADM_ASSEMBLE_H
#define _BCACHEADM_ASSEMBLE_H

extern NihOption opts_assemble[];
int cmd_assemble(NihCommand *, char * const *);

extern NihOption opts_incremental[];
int cmd_incremental(NihCommand *, char * const *);

#endif /* _BCACHEADM_ASSEMBLE_H */
