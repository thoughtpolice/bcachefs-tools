
#include <alloca.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>

#include <nih/command.h>
#include <nih/option.h>

#include <linux/bcache-ioctl.h>

#include "bcache.h"
#include "bcacheadm-assemble.h"

NihOption opts_assemble[] = {
	NIH_OPTION_LAST
};

int cmd_assemble(NihCommand *command, char *const *args)
{
	unsigned nr_devs = nr_args(args);

	struct bch_ioctl_assemble *assemble =
		alloca(sizeof(*assemble) + sizeof(__u64) * nr_devs);

	memset(assemble, 0, sizeof(*assemble));
	assemble->nr_devs = nr_devs;

	for (unsigned i = 0; i < nr_devs; i++)
	     assemble->devs[i] = (__u64) args[i];

	int ret = ioctl(bcachectl_open(), BCH_IOCTL_ASSEMBLE, assemble);
	if (ret < 0)
		die("BCH_IOCTL_ASSEMBLE error: %s", strerror(errno));

	return 0;
}

NihOption opts_incremental[] = {
	NIH_OPTION_LAST
};

int cmd_incremental(NihCommand *command, char *const *args)
{
	if (nr_args(args) != 1)
		die("Please supply exactly one device");

	struct bch_ioctl_incremental incremental = {
		.dev = (__u64) args[0],
	};

	int ret = ioctl(bcachectl_open(), BCH_IOCTL_INCREMENTAL, &incremental);
	if (ret < 0)
		die("BCH_IOCTL_INCREMENTAL error: %s", strerror(errno));

	return 0;
}
