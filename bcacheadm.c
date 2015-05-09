/*
 * Authors: Kent Overstreet <kmo@daterainc.com>
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
#include <blkid.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <dirent.h>

#include "bcache.h"
#include "bcacheadm-format.h"
#include "bcacheadm-assemble.h"
#include "bcacheadm-run.h"
#include "bcacheadm-query.h"

#define PACKAGE_NAME "bcacheadm"
#define PACKAGE_VERSION "1.0"
#define PACKAGE_BUGREPORT "linux-bcache@vger.kernel.org"

#if 0
static bool modify_list_attrs = false;
static const char *modify_set_uuid = NULL;
static const char *modify_dev_uuid = NULL;

static NihOption bcache_modify_options[] = {
	{'l', "list", N_("list attributes"), NULL, NULL, &modify_list_attrs, NULL},
	{'u', "set", N_("cacheset uuid"), NULL, "UUID", &modify_set_uuid, NULL},
	{'d', "dev", N_("device uuid"), NULL, "UUID", &modify_dev_uuid, NULL},
	NIH_OPTION_LAST
};

int bcache_modify(NihCommand *command, char *const *args)
{
	char *err;
	char path[MAX_PATH];
	char *attr = args[0];
	char *val = NULL;
	int fd = -1;

	if (modify_list_attrs) {
		sysfs_attr_list();
		return 0;
	}

	if (!modify_set_uuid) {
		printf("Must provide a cacheset uuid\n");
		return -1;
	}

	snprintf(path, MAX_PATH, "%s/%s", cset_dir, modify_set_uuid);

	if(!attr) {
		printf("Must provide the name of an attribute to modify\n");
		goto err;
	}

	enum sysfs_attr type = sysfs_attr_type(attr);

	if (type == -1)
		goto err;
	else if(type == INTERNAL_ATTR)
		strcat(path, "/internal");
	else if(type == CACHE_ATTR) {
		if(modify_dev_uuid) {
			/* searches all cache# for a matching uuid,
			 * path gets modified to the correct cache path */
			char subdir[10] = "/cache";
			err = find_matching_uuid(path, subdir,
					modify_dev_uuid);
			if (err) {
				printf("Failed to find "
					"matching dev %s\n", err);
				goto err;
			} else {
				strcat(path, subdir);
			}
		} else {
			printf("Must provide a device uuid\n");
		}
	}
	/* SET_ATTRs are just in the current dir */

	strcat(path, "/");
	strcat(path, attr);

	val = args[1];
	if (!val) {
		printf("Must provide a value to change the attribute to\n");
		goto err;
	}

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		printf("Unable to open modify attr with path %s\n", path);
		goto err;
	}

	write(fd, val, strlen(val));

err:
	if(fd)
		close(fd);
	return 0;
}
#endif

#define CMD(_command, _usage, _synopsis, _help)				\
{									\
	.command	= #_command,					\
	.usage		= _usage,					\
	.synopsis	= _synopsis,					\
	.help		= _help,					\
	.group		= NULL,						\
	.options	= opts_##_command,				\
	.action		= cmd_##_command,				\
}

static NihCommand commands[] = {
	CMD(format, N_("<list of devices>"),
	    "Create a new bcache volume from one or more devices",
	    N_("format drive[s] for bcache")),

	CMD(assemble, N_("<devices>"),
	    "Assembles one or more devices into a bcache volume",
	    N_("Registers a list of devices")),
	CMD(incremental, N_("<device"),
	    "Incremental assemble bcache volumes",
	    N_("Incrementally registers a single device")),

	CMD(run, N_("<volume>"),
	    "Start a partially assembled volume",
	    N_("Registers a list of devices")),
	CMD(stop, N_("<volume>"),
	    "Stops a running bcache volume",
	    N_("Unregisters a list of devices")),
	CMD(add, N_("<volume> <devices>"),
	    "Adds a list of devices to a volume",
	    N_("Adds a list of devices to a volume")),
	CMD(readd, N_("<volume> <devices>"),
	    "Adds previously used members of a volume",
	    N_("Adds a list of devices to a volume")),
	CMD(remove, N_("<volume> <devices>"),
	    "Removes a device from its volume",
	    N_("Removes a device from its volume")),
	CMD(fail, N_("<volume> <devices>"),
	    "Sets a device to the FAILED state",
	    N_("Sets a device to the FAILED state")),

#if 0
	CMD(modify, N_("<options>"),
	    "Modifies attributes related to the volume",
	    N_("Modifies attributes related to the volume")),
#endif
	CMD(list, N_("list-cachesets"),
	    "Lists cachesets in /sys/fs/bcache",
	    N_("Lists cachesets in /sys/fs/bcache")),
	CMD(query, N_("query <list of devices>"),
	    "Gives info about the superblock of a list of devices",
	    N_("show superblock on each of the listed drive")),
	CMD(status, N_("status <list of devices>"),
	    "Finds the status of the most up to date superblock",
	    N_("Finds the status of the most up to date superblock")),
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
