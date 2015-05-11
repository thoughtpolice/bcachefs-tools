
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nih/command.h>
#include <nih/option.h>

#include <uuid/uuid.h>

#include "bcache.h"
#include "bcacheadm-run.h"

static bool force_data = false;
static bool force_metadata = false;

NihOption opts_run[] = {
	NIH_OPTION_LAST
};

int cmd_run(NihCommand *command, char *const *args)
{
	return 0;
}

NihOption opts_stop[] = {
	NIH_OPTION_LAST
};

int cmd_stop(NihCommand *command, char *const *args)
{
	int bcachefd = open("/dev/bcache_extent0", O_RDWR);
	if (bcachefd < 0)
		die("Can't open bcache device");

	int ret = ioctl(bcachefd, BCH_IOCTL_STOP);
	if (ret < 0)
		die("BCH_IOCTL_STOP error: %s", strerror(errno));

	close(bcachefd);
	return 0;
}

NihOption opts_add[] = {
	NIH_OPTION_LAST
};

int cmd_add(NihCommand *command, char *const *args)
{
	if (nr_args(args) != 1)
		die("Please supply exactly one device");

	int ret, bcachefd;

	bcachefd = open("/dev/bcache_extent0", O_RDWR);
	if (bcachefd < 0)
		die("Can't open bcache device: %s", strerror(errno));

	struct bch_ioctl_disk_add ia = {
		.dev = (__u64) args[0],
	};

	ret = ioctl(bcachefd, BCH_IOCTL_DISK_ADD, &ia);
	if (ret < 0)
		die("BCH_IOCTL_DISK_ADD error: %s", strerror(ret));

	close(bcachefd);
	return 0;
}

NihOption opts_readd[] = {
	NIH_OPTION_LAST
};

int cmd_readd(NihCommand *command, char *const *args)
{
	if (nr_args(args) != 1)
		die("Please supply exactly one device");

	return 0;
}

NihOption opts_remove[] = {
	{
		'f', "force", N_("force if data present"),
		NULL, NULL, &force_data, NULL
	},
	{
		'\0', "force-metadata", N_("force if metadata present"),
		NULL, NULL, &force_metadata, NULL},
	NIH_OPTION_LAST
};

int cmd_remove(NihCommand *command, char *const *args)
{
	if (nr_args(args) != 1)
		die("Please supply exactly one device");

	int bcachefd = open("/dev/bcache_extent0", O_RDWR);
	if (bcachefd < 0)
		die("Can't open bcache device");

	struct bch_ioctl_disk_remove ir = {
		.dev = (__u64) args[0],
	};

	if (force_data)
		ir.flags |= BCH_FORCE_IF_DATA_MISSING;
	if (force_metadata)
		ir.flags |= BCH_FORCE_IF_METADATA_MISSING;

	int ret = ioctl(bcachefd, BCH_IOCTL_DISK_REMOVE, &ir);
	if (ret < 0)
		die("BCH_IOCTL_DISK_REMOVE error: %s\n", strerror(errno));

	close(bcachefd);
	return 0;
}

static const char *dev_failed_uuid = NULL;

NihOption opts_fail[] = {
	{'d', "dev", N_("dev UUID"), NULL, "UUID", &dev_failed_uuid, NULL},
	NIH_OPTION_LAST
};

int cmd_fail(NihCommand *command, char *const *args)
{
	if (nr_args(args) != 1)
		die("Please supply exactly one device");

	int bcachefd = open("/dev/bcache_extent0", O_RDWR);
	if (bcachefd < 0)
		die("Can't open bcache device");

	struct bch_ioctl_disk_fail df = {
		.dev = (__u64) args[0],
	};

	int ret = ioctl(bcachefd, BCH_IOCTL_DISK_FAIL, &df);
	if (ret < 0)
		die("BCH_IOCTL_DISK_FAIL error: %s\n", strerror(errno));

	return 0;
}
