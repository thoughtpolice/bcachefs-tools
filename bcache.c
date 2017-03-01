/*
 * Authors: Kent Overstreet <kent.overstreet@gmail.com>
 *	    Gabriel de Perthuis <g2p.code@gmail.com>
 *	    Jacob Malevich <jam@datera.io>
 *
 * GPLv2
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cmds.h"

static void usage(void)
{
	puts("bcache - tool for managing bcache volumes/filesystems\n"
	     "usage: bcache <command> [<args>]\n"
	     "\n"
	     "Commands for formatting, startup and shutdown:\n"
	     "  format         Format a new filesystem\n"
	     "  unlock         Unlock an encrypted filesystem prior to running/mounting\n"
	     "  assemble       Assemble an existing multi device filesystem\n"
	     "  incremental    Incrementally assemble an existing multi device filesystem\n"
	     "  run            Start a partially assembled filesystem\n"
	     "  stop	       Stop a running filesystem\n"
	     "\n"
	     "Commands for managing a running filesystem:\n"
	     "  fs_show        Show various information about a filesystem\n"
	     "  fs_set         Modify filesystem options\n"
	     "\n"
	     "Commands for managing a specific device in a filesystem:\n"
	     "  device_show    Show information about a formatted device\n"
	     "  device_add     Add a device to an existing (running) filesystem\n"
	     "  device_remove  Remove a device from an existing (running) filesystem\n"
	     "\n"
	     "Repair:\n"
	     "  bcache fsck    Check an existing filesystem for errors\n"
	     "\n"
	     "Debug:\n"
	     "  bcache dump    Dump filesystem metadata to a qcow2 image\n"
	     "  bcache list    List filesystem metadata in textual form\n"
	     "\n"
	     "Migrate:\n"
	     "  bcache migrate Migrate an existing filesystem to bcachefs, in place\n"
	     "  bcache migrate_superblock\n"
	     "                 Add default superblock, after bcache migrate\n");
}

int main(int argc, char *argv[])
{
	char *cmd;

	setvbuf(stdout, NULL, _IOLBF, 0);

	if (argc < 2) {
		printf("%s: missing command\n", argv[0]);
		usage();
		exit(EXIT_FAILURE);
	}

	cmd = argv[1];

	memmove(&argv[1], &argv[2], argc * sizeof(argv[0]));
	argc--;

	if (!strcmp(cmd, "format"))
		return cmd_format(argc, argv);
	if (!strcmp(cmd, "assemble"))
		return cmd_assemble(argc, argv);
	if (!strcmp(cmd, "incremental"))
		return cmd_incremental(argc, argv);
	if (!strcmp(cmd, "run"))
		return cmd_run(argc, argv);
	if (!strcmp(cmd, "stop"))
		return cmd_stop(argc, argv);

	if (!strcmp(cmd, "fs_show"))
		return cmd_fs_show(argc, argv);
	if (!strcmp(cmd, "fs_set"))
		return cmd_fs_set(argc, argv);

	if (!strcmp(cmd, "device_show"))
		return cmd_device_show(argc, argv);
	if (!strcmp(cmd, "device_add"))
		return cmd_device_add(argc, argv);
	if (!strcmp(cmd, "device_remove"))
		return cmd_device_remove(argc, argv);

	if (!strcmp(cmd, "fsck"))
		return cmd_fsck(argc, argv);

	if (!strcmp(cmd, "unlock"))
		return cmd_unlock(argc, argv);

	if (!strcmp(cmd, "dump"))
		return cmd_dump(argc, argv);
	if (!strcmp(cmd, "list"))
		return cmd_list(argc, argv);

	if (!strcmp(cmd, "migrate"))
		return cmd_migrate(argc, argv);
	if (!strcmp(cmd, "migrate_superblock"))
		return cmd_migrate_superblock(argc, argv);

	usage();
	return 0;
}
