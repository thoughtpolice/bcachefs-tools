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
	char *ioctl = argv[2];

	if (argc < 3) {
		fprintf(stderr, "Enter bcache device and an ioctl to issue\n");
		exit(EXIT_FAILURE);
	}

	bcachefd = open(argv[1], O_RDWR);
	if (bcachefd < 0) {
		perror("Can't open bcache device");
		exit(EXIT_FAILURE);
	}

	argc -= 3;
	argv += 3;

	if (!strcmp(ioctl, "register_devices"))
		return register_devices(argc, argv);
	else {
		fprintf(stderr, "Unknown ioctl\n");
		exit(EXIT_FAILURE);
	}
}

