/*
 * Authors: Kent Overstreet <kent.overstreet@gmail.com>
 *	    Gabriel de Perthuis <g2p.code@gmail.com>
 *	    Jacob Malevich <jam@datera.io>
 *
 * GPLv2
 */

#include <nih/option.h>
#include <nih/command.h>
#include <nih/main.h>
#include <nih/logging.h>
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

#include "bcache.h"
#include "bcache-assemble.h"
#include "bcache-device.h"
#include "bcache-format.h"
#include "bcache-fs.h"
#include "bcache-run.h"

#define PACKAGE_NAME "bcache"
#define PACKAGE_VERSION "1.0"
#define PACKAGE_BUGREPORT "linux-bcache@vger.kernel.org"

const char * const cache_state[] = {
	"active",
	"ro",
	"failed",
	"spare",
	NULL
};

const char * const replacement_policies[] = {
	"lru",
	"fifo",
	"random",
	NULL
};

const char * const csum_types[] = {
	"none",
	"crc32c",
	"crc64",
	NULL
};

const char * const compression_types[] = {
	"none",
	"lzo1x",
	"gzip",
	"xz",
	NULL
};

const char * const error_actions[] = {
	"continue",
	"readonly",
	"panic",
	NULL
};

const char * const bdev_cache_mode[] = {
	"writethrough",
	"writeback",
	"writearound",
	"none",
	NULL
};

const char * const bdev_state[] = {
	"detached",
	"clean",
	"dirty",
	"inconsistent",
	NULL
};

#define CMD(_command, _usage, _synopsis)				\
{									\
	.command	= #_command,					\
	.usage		= _usage,					\
	.synopsis	= _synopsis,					\
	.help		= NULL,						\
	.group		= NULL,						\
	.options	= opts_##_command,				\
	.action		= cmd_##_command,				\
}

static NihCommand commands[] = {
	CMD(format, N_("<list of devices>"),
	    "Create a new bcache volume from one or more devices"),

	/* Bringup, shutdown */

	CMD(assemble, N_("<devices>"),
	    "Assembles one or more devices into a bcache volume"),
	CMD(incremental, N_("<device"),
	    "Incrementally assemble a bcache filesystem"),
	CMD(run, N_("<volume>"),
	    "Start a partially assembled volume"),
	CMD(stop, N_("<volume>"),
	    "Stops a running bcache volume"),

	/* Filesystem commands: */

	CMD(fs_show, N_("<fs>"),
	    "Show information about a filesystem"),
	CMD(fs_set, N_("<fs>"),
	    "Change various filesystem options"),

	/* Device commands: */

	CMD(device_show, N_("<fs>"),
	    "Show information about component devices of a filesystem"),
	CMD(device_add, N_("<volume> <devices>"),
	    "Adds a list of devices to a volume"),
	CMD(device_remove, N_("<volume> <devices>"),
	    "Removes a device from its volume"),

#if 0
	CMD(modify, N_("<options>"),
	    "Modifies attributes related to the volume",
	    N_("Modifies attributes related to the volume")),
	CMD(list, N_("list-cachesets"),
	    "Lists cachesets in /sys/fs/bcache"),
	CMD(query, N_("query <list of devices>"),
	    "Gives info about the superblock of a list of devices"),
	CMD(status, N_("status <list of devices>"),
	    "Finds the status of the most up to date superblock"),
#endif
	NIH_COMMAND_LAST
};

static NihOption options[] = {
	NIH_OPTION_LAST
};

int main(int argc, char *argv[])
{
	nih_main_init(argv[0]);
	nih_option_set_synopsis(_("Manage bcache devices"));
	nih_option_set_help( _("Helps you manage bcache devices"));

	int ret = nih_command_parser(NULL, argc, argv, options, commands);
	if (ret < 0)
		exit(EXIT_FAILURE);

	nih_signal_reset();

	return 0;
}
