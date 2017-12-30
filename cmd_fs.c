
#include <stdio.h>
#include <sys/ioctl.h>

#include <uuid/uuid.h>

#include "libbcachefs/bcachefs_ioctl.h"
#include "libbcachefs/opts.h"

#include "cmds.h"
#include "libbcachefs.h"

static void print_fs_usage(const char *path, enum units units)
{
	unsigned i, j;
	char uuid[40];

	struct bchfs_handle fs = bcache_fs_open(path);
	struct bch_ioctl_usage *u = bchu_usage(fs);

	uuid_unparse(fs.uuid.b, uuid);
	printf("Filesystem %s:\n", uuid);

	printf("%-20s%12s\n", "Size:", pr_units(u->fs.capacity, units));
	printf("%-20s%12s\n", "Used:", pr_units(u->fs.used, units));

	printf("%-20s%12s%12s%12s%12s\n",
	       "By replicas:", "1x", "2x", "3x", "4x");

	for (j = BCH_DATA_BTREE; j < BCH_DATA_NR; j++) {
		printf_pad(20, "  %s:", bch2_data_types[j]);

		for (i = 0; i < BCH_REPLICAS_MAX; i++)
			printf("%12s", pr_units(u->fs.sectors[j][i], units));
		printf("\n");
	}

	printf_pad(20, "  %s:", "reserved");
	for (i = 0; i < BCH_REPLICAS_MAX; i++)
		printf("%12s", pr_units(u->fs.persistent_reserved[i], units));
	printf("\n");

	printf("%-20s%12s\n", "  online reserved:", pr_units(u->fs.online_reserved, units));

	for (i = 0; i < u->nr_devices; i++) {
		struct bch_ioctl_dev_usage *d = u->devs + i;
		char *name = NULL;

		if (!d->alive)
			continue;

		printf("\n");
		printf_pad(20, "Device %u usage:", i);
		name = !d->dev ? strdup("(offline)")
			: dev_to_path(d->dev)
			?: strdup("(device not found)");

		printf("%24s%12s\n", name, bch2_dev_state[d->state]);
		free(name);

		printf("%-20s%12s%12s%12s\n",
		       "", "data", "buckets", "fragmented");

		for (j = BCH_DATA_SB; j < BCH_DATA_NR; j++) {
			u64 frag = max((s64) d->buckets[j] * d->bucket_size -
				       (s64) d->sectors[j], 0LL);

			printf_pad(20, "  %s:", bch2_data_types[j]);
			printf("%12s%12llu%12s\n",
			       pr_units(d->sectors[j], units),
			       d->buckets[j],
			       pr_units(frag, units));
		}
	}

	free(u);
	bcache_fs_close(fs);
}

int cmd_fs_usage(int argc, char *argv[])
{
	enum units units = BYTES;
	unsigned i;
	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1)
		switch (opt) {
		case 'h':
			units = HUMAN_READABLE;
			break;
		}

	if (argc - optind < 1) {
		print_fs_usage(".", units);
	} else {
		for (i = optind; i < argc; i++)
			print_fs_usage(argv[i], units);
	}

	return 0;
}
