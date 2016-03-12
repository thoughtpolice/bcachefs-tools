#ifndef _BCACHE_ASSEMBLE_H
#define _BCACHE_ASSEMBLE_H

extern NihOption opts_assemble[];
int cmd_assemble(NihCommand *, char * const *);

extern NihOption opts_incremental[];
int cmd_incremental(NihCommand *, char * const *);

#endif /* _BCACHE_ASSEMBLE_H */
