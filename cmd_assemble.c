
#include <alloca.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "bcachefs_ioctl.h"
#include "cmds.h"

int cmd_assemble(int argc, char *argv[])
{
	unsigned nr_devs = argc - 1;

	if (argc <= 1)
		die("Please supply at least one device");

	struct bch_ioctl_assemble *assemble =
		alloca(sizeof(*assemble) + sizeof(__u64) * nr_devs);

	memset(assemble, 0, sizeof(*assemble));
	assemble->nr_devs = nr_devs;

	for (unsigned i = 1; i < argc; i++)
	     assemble->devs[i] = (__u64) argv[i];

	xioctl(bcachectl_open(), BCH_IOCTL_ASSEMBLE, assemble);
	return 0;
}

int cmd_incremental(int argc, char *argv[])
{
	if (argc != 2)
		die("Please supply exactly one device");

	struct bch_ioctl_incremental incremental = {
		.dev = (__u64) argv[1],
	};

	xioctl(bcachectl_open(), BCH_IOCTL_INCREMENTAL, &incremental);
	return 0;
}
