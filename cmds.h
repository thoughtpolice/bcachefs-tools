/*
 * Author: Kent Overstreet <kent.overstreet@gmail.com>
 *
 * GPLv2
 */

#ifndef _CMDS_H
#define _CMDS_H

#include "tools-util.h"

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

int cmd_fsck(int argc, char *argv[]);

int cmd_dump(int argc, char *argv[]);
int cmd_list(int argc, char *argv[]);

int cmd_migrate(int argc, char *argv[]);
int cmd_migrate_superblock(int argc, char *argv[]);

#endif /* _CMDS_H */
