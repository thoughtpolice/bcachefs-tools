#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#define __KERNEL__
#include <linux/bcache-ioctl.h>
#undef __KERNEL__

int bcachefd;

static int register_devices(int argc, char *argv[])
{
	int ret;
	ret = ioctl(bcachefd, BCH_IOCTL_REGISTER, argv);
	if (ret < 0) {
		fprintf(stderr, "ioctl error %d", ret);
		exit(EXIT_FAILURE);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char *ioctl = argv[1];

	if (argc < 3) {
		fprintf(stderr, " <Usage> %s <action> <space separated list of devices>", argv[0]);
		fprintf(stderr, "\n <Help>  Possible actions are: \n");
		fprintf(stderr, "          \t 1. register_devices\n");
		exit(EXIT_FAILURE);
	}

	bcachefd = open("/dev/bcache", O_RDWR);
	if (bcachefd < 0) {
		perror("Can't open bcache device");
		exit(EXIT_FAILURE);
	}

	argc -= 2;
	argv += 2;

	if (!strcmp(ioctl, "register_devices"))
		return register_devices(argc, argv);
	else {
		fprintf(stderr, "Unknown ioctl\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}
