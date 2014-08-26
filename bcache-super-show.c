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

static bool force_csum = false;

static void usage()
{
	fprintf(stderr, "Usage: bcache-super-show [-f] <device>\n");
}

static void print_encode(char *in)
{
	for (char *pos = in; *pos; pos++)
		if (isalnum(*pos) || strchr(".-_", *pos))
			putchar(*pos);
		else
			printf("%%%x", *pos);
}

static void show_super_common(struct cache_sb *sb)
{
	char uuid[40];
	char label[SB_LABEL_SIZE + 1];
	uint64_t expected_csum;

	printf("sb.magic\t\t");
	if (!memcmp(&sb->magic, &BCACHE_MAGIC, sizeof(sb->magic))) {
		printf("ok\n");
	} else {
		printf("bad magic\n");
		fprintf(stderr, "Invalid superblock (bad magic)\n");
		exit(2);
	}

	printf("sb.first_sector\t\t%ju", (uint64_t) sb->offset);
	if (sb->offset == SB_SECTOR) {
		printf(" [match]\n");
	} else {
		printf(" [expected %ds]\n", SB_SECTOR);
		fprintf(stderr, "Invalid superblock (bad sector)\n");
		exit(2);
	}

	printf("sb.csum\t\t\t%ju", (uint64_t) sb->csum);
	expected_csum = csum_set(sb,
				 sb->version < BCACHE_SB_VERSION_CDEV_V3
				 ? BCH_CSUM_CRC64
				 : CACHE_SB_CSUM_TYPE(sb));
	if (sb->csum == expected_csum) {
		printf(" [match]\n");
	} else {
		printf(" [expected %" PRIX64 "]\n", expected_csum);
		if (!force_csum) {
			fprintf(stderr, "Corrupt superblock (bad csum)\n");
			exit(2);
		}
	}

	printf("sb.version\t\t%ju", (uint64_t) sb->version);
	switch (sb->version) {
		// These are handled the same by the kernel
		case BCACHE_SB_VERSION_CDEV:
		case BCACHE_SB_VERSION_CDEV_WITH_UUID:
			printf(" [cache device]\n");
			break;

		// The second adds data offset support
		case BCACHE_SB_VERSION_BDEV:
		case BCACHE_SB_VERSION_BDEV_WITH_OFFSET:
			printf(" [backing device]\n");
			break;

		default:
			printf(" [unknown]\n");
			// exit code?
			exit(EXIT_SUCCESS);
	}

	putchar('\n');

	strncpy(label, (char *) sb->label, SB_LABEL_SIZE);
	label[SB_LABEL_SIZE] = '\0';
	printf("dev.label\t\t");
	if (*label)
		print_encode(label);
	else
		printf("(empty)");
	putchar('\n');

	uuid_unparse(sb->uuid.b, uuid);
	printf("dev.uuid\t\t%s\n", uuid);

	uuid_unparse(sb->set_uuid.b, uuid);
	printf("cset.uuid\t\t%s\n", uuid);
}

static void show_super_backingdev(struct cache_sb *sb)
{
	uint64_t first_sector;

	show_super_common(sb);

	if (sb->version == BCACHE_SB_VERSION_BDEV) {
		first_sector = BDEV_DATA_START_DEFAULT;
	} else {
		if (sb->keys == 1 || sb->d[0]) {
			fprintf(stderr,
				"Possible experimental format detected, bailing\n");
			exit(3);
		}
		first_sector = sb->data_offset;
	}

	printf("dev.data.first_sector\t%ju\n"
	       "dev.data.cache_mode\t%s"
	       "dev.data.cache_state\t%s\n",
	       first_sector,
	       bdev_cache_mode[BDEV_CACHE_MODE(sb)],
	       bdev_state[BDEV_STATE(sb)]);
}

static void show_cache_member(struct cache_sb *sb, unsigned i)
{
	struct cache_member *m = ((struct cache_member *) sb->d) + i;

	printf("cache.state\t%s\n",		cache_state[CACHE_STATE(m)]);
	printf("cache.tier\t%llu\n",		CACHE_TIER(m));

	printf("cache.replication_set\t%llu\n",	CACHE_REPLICATION_SET(m));
	printf("cache.cur_meta_replicas\t%llu\n", REPLICATION_SET_CUR_META_REPLICAS(m));
	printf("cache.cur_data_replicas\t%llu\n", REPLICATION_SET_CUR_DATA_REPLICAS(m));

	printf("cache.has_metadata\t%llu\n",	CACHE_HAS_METADATA(m));
	printf("cache.has_data\t%llu\n",	CACHE_HAS_DATA(m));

	printf("cache.replacement\t%s\n",	replacement_policies[CACHE_REPLACEMENT(m)]);
	printf("cache.discard\t%llu\n",		CACHE_DISCARD(m));
}

static void show_super_cache(struct cache_sb *sb)
{
	show_super_common(sb);

	printf("dev.sectors_per_block\t%u\n"
	       "dev.sectors_per_bucket\t%u\n",
	       sb->block_size,
	       sb->bucket_size);

	// total_sectors includes the superblock;
	printf("dev.cache.first_sector\t%u\n"
	       "dev.cache.cache_sectors\t%llu\n"
	       "dev.cache.total_sectors\t%llu\n"
	       "dev.cache.ordered\t%s\n"
	       "dev.cache.pos\t\t%u\n"
	       "dev.cache.setsize\t\t%u\n",
	       sb->bucket_size * sb->first_bucket,
	       sb->bucket_size * (sb->nbuckets - sb->first_bucket),
	       sb->bucket_size * sb->nbuckets,
	       CACHE_SYNC(sb) ? "yes" : "no",
	       sb->nr_this_dev,
	       sb->nr_in_set);

	show_cache_member(sb, sb->nr_this_dev);
}

int main(int argc, char **argv)
{
	int o;
	extern char *optarg;
	struct cache_sb sb_stack, *sb = &sb_stack;
	size_t bytes = sizeof(*sb);

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
		show_super_cache(sb);
	else
		show_super_backingdev(sb);

	return 0;
}
