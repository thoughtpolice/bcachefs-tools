/*
 * Author: Kent Overstreet <kent.overstreet@gmail.com>
 *
 * GPLv2
 */

#ifndef _BCACHE_H
#define _BCACHE_H

#include "util.h"

extern const char * const cache_state[];
extern const char * const replacement_policies[];
extern const char * const csum_types[];
extern const char * const compression_types[];
extern const char * const error_actions[];
extern const char * const bdev_cache_mode[];
extern const char * const bdev_state[];

int cmd_format(int argc, char *argv[]);

int cmd_unlock(int argc, char *argv[]);
int cmd_assemble(int argc, char *argv[]);
int cmd_incremental(int argc, char *argv[]);
int cmd_run(int argc, char *argv[]);
int cmd_stop(int argc, char *argv[]);

int cmd_fs_show(int argc, char *argv[]);
int cmd_fs_set(int argc, char *argv[]);

int cmd_device_show(int argc, char *argv[]);
int cmd_device_add(int argc, char *argv[]);
int cmd_device_remove(int argc, char *argv[]);

#endif /* _BCACHE_H */
