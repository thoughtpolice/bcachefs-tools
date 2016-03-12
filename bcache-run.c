
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
#include "bcache-run.h"

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
	if (nr_args(args) != 1)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(args[0]);

	if (ioctl(fs.fd, BCH_IOCTL_STOP))
		die("BCH_IOCTL_STOP error: %s", strerror(errno));

	return 0;
}
