/*
 * Author: Gabriel de Perthuis <g2p.code@gmail.com>
 *
 * GPLv2
 */


#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 500

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "bcache.h"

void usage()
{
	fprintf(stderr, "Usage: bcache-super-show [-f] <device>\n");
}

int main(int argc, char **argv)
{
	int o;
	extern char *optarg;
	struct cache_sb sb_stack, *sb = &sb_stack;
	size_t bytes = sizeof(*sb);
	bool force_csum = false;

	while ((o = getopt(argc, argv, "f")) != EOF)
		switch (o) {
			case 'f':
				force_csum = 1;
				break;

			default:
				usage();
				exit(1);
		}

	argv += optind;
	argc -= optind;

	if (argc != 1) {
		usage();
		exit(1);
	}

	int fd = open(argv[0], O_RDONLY);
	if (fd < 0) {
		printf("Can't open dev %s: %s\n", argv[0], strerror(errno));
		exit(2);
	}

	if (pread(fd, sb, bytes, SB_START) != bytes) {
		fprintf(stderr, "Couldn't read\n");
		exit(2);
	}

	if (sb->keys) {
		bytes = sizeof(*sb) + sb->keys * sizeof(uint64_t);
		sb = malloc(bytes);

		if (pread(fd, sb, bytes, SB_START) != bytes) {
			fprintf(stderr, "Couldn't read\n");
			exit(2);
		}
	}

	if (!SB_IS_BDEV(sb))
		show_super_cache(sb, force_csum);
	else
		show_super_backingdev(sb, force_csum);

	return 0;
}
