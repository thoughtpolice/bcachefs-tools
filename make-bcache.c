/*
 * Author: Kent Overstreet <kmo@daterainc.com>
 *
 * GPLv2
 */

#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET 64
#define _XOPEN_SOURCE 600

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
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
	fprintf(stderr,
		   "Usage: make-bcache [options] device\n"
	       "	-C, --cache			Format a cache device\n"
	       "	-B, --bdev			Format a backing device\n"
	       "	    --wipe-bcache		destroy existing bcache data if present\n"
	       "	-l, --label			label\n"
	       "	    --cset-uuid			UUID for the cache set\n"
	       "	    --csum-type			One of (none|crc32c|crc64)\n"

	       "	-b, --bucket			bucket size\n"
	       "	-w, --block			block size (hard sector size of SSD, often 2k)\n"

	       "	    --replication-set		replication set of subsequent devices\n"
	       "	    --meta-replicas		number of metadata replicas\n"
	       "	    --data-replicas		number of data replicas\n"
	       "	    --tier			tier of subsequent devices\n"
	       "	    --cache_replacement_policy	one of (lru|fifo|random)\n"
	       "	    --discard			enable discards\n"

	       "	    --writeback			enable writeback\n"
	       "	-o, --data-offset		data offset in sectors\n"
	       "	-h, --help			display this help and exit\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int c, bdev = -1;
	size_t i, nr_backing_devices = 0;

	unsigned block_size = 0, bucket_size = 1024;
	int writeback = 0, discard = 0, wipe_bcache = 0;
	unsigned replication_set = 0, tier = 0, replacement_policy = 0;
	uint64_t data_offset = BDEV_DATA_START_DEFAULT;
	char *label = NULL;

	const char *cache_devices[argc];
	int cache_dev_fd[argc];

	const char *backing_devices[argc];
	int backing_dev_fd[argc];
	const char *backing_dev_labels[argc];

	enum long_opts {
		CACHE_SET_UUID = 256,
		CSUM_TYPE,
		REPLICATION_SET,
		META_REPLICAS,
		DATA_REPLICAS,
	};

	const struct option opts[] = {
		{ "cache",			0, NULL,	'C' },
		{ "bdev",			0, NULL,	'B' },
		{ "wipe-bcache",		0, &wipe_bcache, 1  },
		{ "label",			1, NULL,	'l' },
		{ "cset-uuid",			1, NULL,	CACHE_SET_UUID },
		{ "csum-type", 			1, NULL,	CSUM_TYPE },

		{ "bucket",			1, NULL,	'b' },
		{ "block",			1, NULL,	'w' },

		{ "replication-set",		1, NULL,	REPLICATION_SET },
		{ "meta-replicas",		1, NULL,	META_REPLICAS},
		{ "data-replicas",		1, NULL,	DATA_REPLICAS },
		{ "tier",			1, NULL,	't' },
		{ "cache_replacement_policy",	1, NULL,	'p' },
		{ "discard",			0, &discard,	1   },

		{ "writeback",			0, &writeback,	1   },
		{ "data_offset",		1, NULL,	'o' },

		{ "help",			0, NULL,	'h' },
		{ NULL,				0, NULL,	0 },
	};

	struct cache_sb *cache_set_sb = calloc(1, sizeof(*cache_set_sb) +
				     sizeof(struct cache_member) * argc);

	uuid_generate(cache_set_sb->set_uuid.b);
	SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb, BCH_CSUM_CRC32C);
	SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb, 1);
	SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb, 1);

	while ((c = getopt_long(argc, argv,
				"-hCBU:w:b:l:",
				opts, NULL)) != -1) {

		switch (c) {
		case 'C':
			bdev = 0;
			break;
		case 'B':
			bdev = 1;
			break;
		case 'l':
			label = optarg;
			memcpy(cache_set_sb->label, label,
			       sizeof(cache_set_sb->label));
			break;
		case CACHE_SET_UUID:
			if (uuid_parse(optarg, cache_set_sb->set_uuid.b)) {
				fprintf(stderr, "Bad uuid\n");
				exit(EXIT_FAILURE);
			}
			break;
		case CSUM_TYPE:
			SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb,
				read_string_list_or_die(optarg, csum_types,
							"csum type"));
			break;

		case 'b':
			bucket_size = hatoi_validate(optarg, "bucket size");
			break;
		case 'w':
			block_size = hatoi_validate(optarg, "block size");
			break;

		case REPLICATION_SET:
			replication_set = strtoul_or_die(optarg,
							 CACHE_REPLICATION_SET_MAX,
							 "replication set");
			break;
		case META_REPLICAS:
			SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb,
					strtoul_or_die(optarg, 
						       CACHE_SET_META_REPLICAS_WANT_MAX,
						       "meta replicas"));
			break;
		case DATA_REPLICAS:
			SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb,
					strtoul_or_die(optarg, 
						       CACHE_SET_DATA_REPLICAS_WANT_MAX,
						       "data replicas"));
			break;
		case 't':
			tier = strtoul_or_die(optarg, CACHE_TIERS, "tier");
			break;
		case 'p':
			replacement_policy = read_string_list_or_die(optarg,
							replacement_policies,
							"cache replacement policy");
			break;

		case 'o':
			data_offset = atoll(optarg);
			if (data_offset < BDEV_DATA_START_DEFAULT) {
				fprintf(stderr, "Bad data offset; minimum %d sectors\n",
				       BDEV_DATA_START_DEFAULT);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage();
			break;
		case 1:
			if (bdev == -1) {
				fprintf(stderr, "Please specify -C or -B\n");
				exit(EXIT_FAILURE);
			}

			if (bdev) {
				backing_dev_labels[nr_backing_devices] = label;
				backing_devices[nr_backing_devices++] = optarg;
			} else {
				cache_devices[cache_set_sb->nr_in_set] = optarg;
				next_cache_device(cache_set_sb,
						  replication_set,
						  tier,
						  replacement_policy,
						  discard);
			}

			break;
		}
	}

	if (!cache_set_sb->nr_in_set && !nr_backing_devices) {
		fprintf(stderr, "Please supply a device\n");
		usage();
	}

	if (bucket_size < block_size) {
		fprintf(stderr, "Bucket size cannot be smaller than block size\n");
		exit(EXIT_FAILURE);
	}

	if (!block_size) {
		for (i = 0; i < cache_set_sb->nr_in_set; i++)
			block_size = max(block_size,
					 get_blocksize(cache_devices[i]));

		for (i = 0; i < nr_backing_devices; i++)
			block_size = max(block_size,
					 get_blocksize(backing_devices[i]));
	}

	for (i = 0; i < cache_set_sb->nr_in_set; i++)
		cache_dev_fd[i] = dev_open(cache_devices[i], wipe_bcache);

	for (i = 0; i < nr_backing_devices; i++)
		backing_dev_fd[i] = dev_open(backing_devices[i], wipe_bcache);

	write_cache_sbs(cache_dev_fd, cache_set_sb, block_size, bucket_size);

	for (i = 0; i < nr_backing_devices; i++)
		write_backingdev_sb(backing_dev_fd[i],
				    block_size, bucket_size,
				    writeback, data_offset,
				    backing_dev_labels[i],
				    cache_set_sb->set_uuid);

	return 0;
}
