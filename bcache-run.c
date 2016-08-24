
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nih/option.h>

#include <uuid/uuid.h>

#include "bcache.h"

int cmd_run(int argc, char *argv[])
{
	NihOption opts[] = {
		NIH_OPTION_LAST
	};
	bch_nih_init(argc, argv, opts);

	return 0;
}

int cmd_stop(int argc, char *argv[])
{
	NihOption opts[] = {
		NIH_OPTION_LAST
	};
	char **args = bch_nih_init(argc, argv, opts);

	if (nr_args(args) != 1)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(args[0]);

	if (ioctl(fs.fd, BCH_IOCTL_STOP))
		die("BCH_IOCTL_STOP error: %s", strerror(errno));

	return 0;
}
