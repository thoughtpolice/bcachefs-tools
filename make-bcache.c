/*
 * Author: Kent Overstreet <kmo@daterainc.com>
 *
 * GPLv2
 */

#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 600

#include <blkid.h>
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

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

uint64_t getblocks(int fd)
{
	uint64_t ret;
	struct stat statbuf;
	if (fstat(fd, &statbuf)) {
		perror("stat error\n");
		exit(EXIT_FAILURE);
	}
	ret = statbuf.st_size / 512;
	if (S_ISBLK(statbuf.st_mode))
		if (ioctl(fd, BLKGETSIZE, &ret)) {
			perror("ioctl error");
			exit(EXIT_FAILURE);
		}
	return ret;
}

uint64_t hatoi(const char *s)
{
	char *e;
	long long i = strtoll(s, &e, 10);
	switch (*e) {
		case 't':
		case 'T':
			i *= 1024;
		case 'g':
		case 'G':
			i *= 1024;
		case 'm':
		case 'M':
			i *= 1024;
		case 'k':
		case 'K':
			i *= 1024;
	}
	return i;
}

unsigned hatoi_validate(const char *s, const char *msg)
{
	uint64_t v = hatoi(s);

	if (v & (v - 1)) {
		fprintf(stderr, "%s must be a power of two\n", msg);
		exit(EXIT_FAILURE);
	}

	v /= 512;

	if (v > USHRT_MAX) {
		fprintf(stderr, "%s too large\n", msg);
		exit(EXIT_FAILURE);
	}

	if (!v) {
		fprintf(stderr, "%s too small\n", msg);
		exit(EXIT_FAILURE);
	}

	return v;
}

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

static void do_write_sb(int fd, struct cache_sb *sb)
{
	char zeroes[SB_START] = {0};
	size_t bytes = ((void *) bset_bkey_last(sb)) - (void *) sb;

	/* Zero start of disk */
	if (pwrite(fd, zeroes, SB_START, 0) != SB_START) {
		perror("write error\n");
		exit(EXIT_FAILURE);
	}
	/* Write superblock */
	if (pwrite(fd, sb, bytes, SB_START) != bytes) {
		perror("write error\n");
		exit(EXIT_FAILURE);
	}

	fsync(fd);
	close(fd);
}

static void write_backingdev_sb(int fd, unsigned block_size, unsigned bucket_size,
				bool writeback, uint64_t data_offset,
				const char *label,
				uuid_le set_uuid)
{
	char uuid_str[40], set_uuid_str[40];
	struct cache_sb sb;

	memset(&sb, 0, sizeof(struct cache_sb));

	sb.offset	= SB_SECTOR;
	sb.version	= BCACHE_SB_VERSION_BDEV;
	sb.magic	= BCACHE_MAGIC;
	uuid_generate(sb.uuid.b);
	sb.set_uuid	= set_uuid;
	sb.bucket_size	= bucket_size;
	sb.block_size	= block_size;

	uuid_unparse(sb.uuid.b, uuid_str);
	uuid_unparse(sb.set_uuid.b, set_uuid_str);
	if (label)
		memcpy(sb.label, label, SB_LABEL_SIZE);

	SET_BDEV_CACHE_MODE(&sb, writeback
			    ? CACHE_MODE_WRITEBACK
			    : CACHE_MODE_WRITETHROUGH);

	if (data_offset != BDEV_DATA_START_DEFAULT) {
		sb.version = BCACHE_SB_VERSION_BDEV_WITH_OFFSET;
		sb.data_offset = data_offset;
	}

	sb.csum = csum_set(&sb, BCH_CSUM_CRC64);

	printf("UUID:			%s\n"
	       "Set UUID:		%s\n"
	       "version:		%u\n"
	       "block_size:		%u\n"
	       "data_offset:		%ju\n",
	       uuid_str, set_uuid_str,
	       (unsigned) sb.version,
	       sb.block_size,
	       data_offset);

	do_write_sb(fd, &sb);
}

static int dev_open(const char *dev, bool wipe_bcache)
{
	struct cache_sb sb;
	blkid_probe pr;
	int fd;

	if ((fd = open(dev, O_RDWR|O_EXCL)) == -1) {
		fprintf(stderr, "Can't open dev %s: %s\n", dev, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (pread(fd, &sb, sizeof(sb), SB_START) != sizeof(sb))
		exit(EXIT_FAILURE);

	if (!memcmp(&sb.magic, &BCACHE_MAGIC, 16) && !wipe_bcache) {
		fprintf(stderr, "Already a bcache device on %s, "
			"overwrite with --wipe-bcache\n", dev);
		exit(EXIT_FAILURE);
	}

	if (!(pr = blkid_new_probe()))
		exit(EXIT_FAILURE);
	if (blkid_probe_set_device(pr, fd, 0, 0))
		exit(EXIT_FAILURE);
	/* enable ptable probing; superblock probing is enabled by default */
	if (blkid_probe_enable_partitions(pr, true))
		exit(EXIT_FAILURE);
	if (!blkid_do_probe(pr)) {
		/* XXX wipefs doesn't know how to remove partition tables */
		fprintf(stderr, "Device %s already has a non-bcache superblock, "
				"remove it using wipefs and wipefs -a\n", dev);
		exit(EXIT_FAILURE);
	}

	return fd;
}

static void write_cache_sbs(int *fds, struct cache_sb *sb,
			    unsigned block_size, unsigned bucket_size)
{
	char uuid_str[40], set_uuid_str[40];
	size_t i;

	sb->offset	= SB_SECTOR;
	sb->version	= BCACHE_SB_VERSION_CDEV_V3;
	sb->magic	= BCACHE_MAGIC;
	sb->bucket_size	= bucket_size;
	sb->block_size	= block_size;
	sb->keys	= bch_journal_buckets_offset(sb);

	/*
	 * don't have a userspace crc32c implementation handy, just always use
	 * crc64
	 */
	SET_CACHE_SB_CSUM_TYPE(sb, BCH_CSUM_CRC64);

	for (i = 0; i < sb->nr_in_set; i++) {
		struct cache_member *m = sb->members + i;

		sb->uuid = m->uuid;

		sb->nbuckets		= getblocks(fds[i]) / sb->bucket_size;
		sb->nr_this_dev		= i;
		sb->first_bucket	= (23 / sb->bucket_size) + 1;

		if (sb->nbuckets < 1 << 7) {
			fprintf(stderr, "Not enough buckets: %llu, need %u\n",
				sb->nbuckets, 1 << 7);
			exit(EXIT_FAILURE);
		}

		sb->csum = csum_set(sb, CACHE_SB_CSUM_TYPE(sb));

		uuid_unparse(sb->uuid.b, uuid_str);
		uuid_unparse(sb->set_uuid.b, set_uuid_str);
		printf("UUID:			%s\n"
		       "Set UUID:		%s\n"
		       "version:		%u\n"
		       "nbuckets:		%llu\n"
		       "block_size:		%u\n"
		       "bucket_size:		%u\n"
		       "nr_in_set:		%u\n"
		       "nr_this_dev:		%u\n"
		       "first_bucket:		%u\n",
		       uuid_str, set_uuid_str,
		       (unsigned) sb->version,
		       sb->nbuckets,
		       sb->block_size,
		       sb->bucket_size,
		       sb->nr_in_set,
		       sb->nr_this_dev,
		       sb->first_bucket);

		do_write_sb(fds[i], sb);
	}
}

static void next_cache_device(struct cache_sb *sb,
			      unsigned replication_set,
			      unsigned tier,
			      unsigned replacement_policy,
			      bool discard)
{
	struct cache_member *m = sb->members + sb->nr_in_set;

	SET_CACHE_REPLICATION_SET(m, replication_set);
	SET_CACHE_TIER(m, tier);
	SET_CACHE_REPLACEMENT(m, replacement_policy);
	SET_CACHE_DISCARD(m, discard);
	uuid_generate(m->uuid.b);

	sb->nr_in_set++;
}

static unsigned get_blocksize(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf)) {
		fprintf(stderr, "Error statting %s: %s\n",
			path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (S_ISBLK(statbuf.st_mode)) {
		/* check IO limits:
		 * BLKALIGNOFF: alignment_offset
		 * BLKPBSZGET: physical_block_size
		 * BLKSSZGET: logical_block_size
		 * BLKIOMIN: minimum_io_size
		 * BLKIOOPT: optimal_io_size
		 *
		 * It may be tempting to use physical_block_size,
		 * or even minimum_io_size.
		 * But to be as transparent as possible,
		 * we want to use logical_block_size.
		 */
		unsigned int logical_block_size;
		int fd = open(path, O_RDONLY);

		if (fd < 0) {
			fprintf(stderr, "open(%s) failed: %m\n", path);
			exit(EXIT_FAILURE);
		}
		if (ioctl(fd, BLKSSZGET, &logical_block_size)) {
			fprintf(stderr, "ioctl(%s, BLKSSZGET) failed: %m\n", path);
			exit(EXIT_FAILURE);
		}
		close(fd);
		return logical_block_size / 512;

	}
	/* else: not a block device.
	 * Why would we even want to write a bcache super block there? */

	return statbuf.st_blksize / 512;
}

static long strtoul_or_die(const char *p, size_t max, const char *msg)
{
	errno = 0;
	long v = strtol(optarg, NULL, 10);
	if (errno || v < 0 || v >= max) {
		fprintf(stderr, "Invalid %s %zi\n", msg, v);
		exit(EXIT_FAILURE);
	}

	return v;
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
