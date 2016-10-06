
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <uuid/uuid.h>

#include "bcache.h"

int cmd_run(int argc, char *argv[])
{
	return 0;
}

int cmd_stop(int argc, char *argv[])
{
	if (argc != 2)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(argv[1]);

	if (ioctl(fs.fd, BCH_IOCTL_STOP))
		die("BCH_IOCTL_STOP error: %s", strerror(errno));

	return 0;
}
