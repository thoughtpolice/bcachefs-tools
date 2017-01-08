/*
 * Author: Kent Overstreet <kent.overstreet@gmail.com>
 *
 * GPLv2
 */

#ifndef _BCACHE_H
#define _BCACHE_H

#include "tools-util.h"

int cmd_format(int argc, char *argv[]);

int cmd_assemble(int argc, char *argv[]);
int cmd_incremental(int argc, char *argv[]);
int cmd_run(int argc, char *argv[]);
int cmd_stop(int argc, char *argv[]);

int cmd_fs_show(int argc, char *argv[]);
int cmd_fs_set(int argc, char *argv[]);

int cmd_device_show(int argc, char *argv[]);
int cmd_device_add(int argc, char *argv[]);
int cmd_device_remove(int argc, char *argv[]);

int cmd_fsck(int argc, char *argv[]);

#endif /* _BCACHE_H */
