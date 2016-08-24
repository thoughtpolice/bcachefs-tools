
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <nih/option.h>

#include "bcache.h"

struct bcache_dev {
	unsigned	nr;
	const char	*name;

	unsigned	has_metadata;
	unsigned	has_data;
	const char	*state;
	unsigned	tier;

	u64		bucket_size;
	u64		first_bucket;
	u64		nbuckets;

	/* XXX: dirty != used, it doesn't count metadata */
	u64		bytes_dirty;
};

static struct bcache_dev fill_dev(const char *dev_name, unsigned nr, int dir)
{
	return (struct bcache_dev) {
		.nr		= nr,
		.name		= dev_name,

		.has_metadata	= read_file_u64(dir, "has_metadata"),
		.has_data	= read_file_u64(dir, "has_data"),
		.state		= read_file_str(dir, "state"),
		.tier		= read_file_u64(dir, "tier"),

		.bucket_size	= read_file_u64(dir, "bucket_size_bytes"),
		.first_bucket	= read_file_u64(dir, "first_bucket"),
		.nbuckets	= read_file_u64(dir, "nbuckets"),
		.bytes_dirty	= read_file_u64(dir, "dirty_bytes"),
	};
}

static void show_dev(struct bcache_dev *dev)
{
	u64 capacity = (dev->nbuckets - dev->first_bucket) *
		dev->bucket_size;
	/*
	 * XXX: show fragmentation information, cached/dirty information
	 */

	printf("Device %u (/dev/%s):\n"
	       "    Has metadata:\t%u\n"
	       "    Has dirty data:\t%u\n"
	       "    State:\t\t%s\n"
	       "    Tier:\t\t%u\n"
	       "    Size:\t\t%llu\n"
	       "    Used:\t\t%llu\n"
	       "    Free:\t\t%llu\n"
	       "    Use%%:\t\t%llu\n",
	       dev->nr, dev->name,
	       dev->has_metadata,
	       dev->has_data,
	       dev->state,
	       dev->tier,
	       capacity,
	       dev->bytes_dirty,
	       capacity - dev->bytes_dirty,
	       (dev->bytes_dirty * 100) / capacity);
}

int cmd_device_show(int argc, char *argv[])
{
	int human_readable = 0;
	NihOption opts[] = {
	//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}

		{ 'h',	"human-readable",		N_("print sizes in powers of 1024 (e.g., 1023M)"),
			NULL, NULL,	&human_readable,	NULL},
		NIH_OPTION_LAST
	};
	char **args = bch_nih_init(argc, argv, opts);

	if (nr_args(args) != 1)
		die("Please supply a single device");

	struct bcache_handle fs = bcache_fs_open(args[0]);
	struct dirent *entry;

	struct bcache_dev devices[256];
	unsigned i, j, nr_devices = 0, nr_active_tiers = 0;

	unsigned tiers[CACHE_TIERS]; /* number of devices in each tier */
	memset(tiers, 0, sizeof(tiers));

	while ((entry = readdir(fs.sysfs))) {
		unsigned nr;
		int pos = 0;

		sscanf(entry->d_name, "cache%u%n", &nr, &pos);
		if (pos != strlen(entry->d_name))
			continue;

		char link[PATH_MAX];
		if (readlinkat(dirfd(fs.sysfs), entry->d_name,
			       link, sizeof(link)) < 0)
			die("readlink error: %s\n", strerror(errno));

		char *dev_name = basename(dirname(link));

		int fd = openat(dirfd(fs.sysfs), entry->d_name, O_RDONLY);
		if (fd < 0)
			die("couldn't open device %s: %s\n",
			    entry->d_name, strerror(errno));

		devices[nr_devices] = fill_dev(strdup(dev_name), nr, fd);
		tiers[devices[nr_devices].tier]++;
		nr_devices++;

		close(fd);
	}

	for (i = 0; i < CACHE_TIERS; i++)
		if (tiers[i])
			nr_active_tiers++;

	/* Print out devices sorted by tier: */
	bool first = true;

	for (i = 0; i < CACHE_TIERS; i++) {
		if (!tiers[i])
			continue;

		if (nr_active_tiers > 1) {
			if (!first)
				printf("\n");
			first = false;
			printf("Tier %u:\n\n", i);
		}

		for (j = 0; j < nr_devices; j++) {
			if (devices[j].tier != i)
				continue;

			if (!first)
				printf("\n");
			first = false;
			show_dev(&devices[j]);
		}
	}

	return 0;
}

int cmd_device_add(int argc, char *argv[])
{
	NihOption opts[] = {
	//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}
		NIH_OPTION_LAST
	};
	char **args = bch_nih_init(argc, argv, opts);

	if (nr_args(args) < 2)
		die("Please supply a filesystem and at least one device to add");

	struct bcache_handle fs = bcache_fs_open(args[0]);

	for (unsigned i = 1; args[i]; i++) {
		struct bch_ioctl_disk_add ia = {
			.dev = (__u64) args[i],
		};

		if (ioctl(fs.fd, BCH_IOCTL_DISK_ADD, &ia))
			die("BCH_IOCTL_DISK_ADD error: %s", strerror(errno));
	}

	return 0;
}

int cmd_device_remove(int argc, char *argv[])
{
	int force_data = 0, force_metadata = 0;
	NihOption opts[] = {
	//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}

		{ 'f', "force",			N_("force if data present"),
			NULL, NULL,	&force_data, NULL },
		{ '\0', "force-metadata",	N_("force if metadata present"),
			NULL, NULL,	&force_metadata, NULL},
		NIH_OPTION_LAST
	};
	char **args = bch_nih_init(argc, argv, opts);

	if (nr_args(args) < 2)
		die("Please supply a filesystem and at least one device to add");

	struct bcache_handle fs = bcache_fs_open(args[0]);

	for (unsigned i = 1; args[i]; i++) {
		struct bch_ioctl_disk_remove ir = {
			.dev = (__u64) args[0],
		};

		if (force_data)
			ir.flags |= BCH_FORCE_IF_DATA_MISSING;
		if (force_metadata)
			ir.flags |= BCH_FORCE_IF_METADATA_MISSING;

		if (ioctl(fs.fd, BCH_IOCTL_DISK_REMOVE, &ir))
			die("BCH_IOCTL_DISK_REMOVE error: %s\n", strerror(errno));
	}

	return 0;
}
