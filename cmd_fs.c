
#include <stdio.h>
#include <sys/ioctl.h>

#include <uuid/uuid.h>

#include "ccan/darray/darray.h"

#include "linux/sort.h"

#include "libbcachefs/bcachefs_ioctl.h"
#include "libbcachefs/opts.h"

#include "cmds.h"
#include "libbcachefs.h"

static void print_dev_usage(struct bch_ioctl_dev_usage *d, unsigned idx,
			    const char *label, enum units units)
{
	char *name = NULL;
	u64 available = d->nr_buckets;
	unsigned i;

	printf("\n");
	printf_pad(20, "%s (device %u):", label, idx);

	name = !d->dev ? strdup("(offline)")
		: dev_to_path(d->dev)
		?: strdup("(device not found)");
	printf("%24s%12s\n", name, bch2_dev_state[d->state]);
	free(name);

	printf("%-20s%12s%12s%12s\n",
	       "", "data", "buckets", "fragmented");

	for (i = BCH_DATA_SB; i < BCH_DATA_NR; i++) {
		u64 frag = max((s64) d->buckets[i] * d->bucket_size -
			       (s64) d->sectors[i], 0LL);

		printf_pad(20, "  %s:", bch2_data_types[i]);
		printf("%12s%12llu%12s\n",
		       pr_units(d->sectors[i], units),
		       d->buckets[i],
		       pr_units(frag, units));

		if (i != BCH_DATA_CACHED)
			available -= d->buckets[i];
	}

	printf_pad(20, "  available:");
	printf("%12s%12llu\n",
	       pr_units(available * d->bucket_size, units),
	       available);

	printf_pad(20, "  capacity:");
	printf("%12s%12llu\n",
	       pr_units(d->nr_buckets * d->bucket_size, units),
	       d->nr_buckets);
}

struct dev_by_label {
	unsigned	idx;
	char		*label;
};

static int dev_by_label_cmp(const void *_l, const void *_r)
{
	const struct dev_by_label *l = _l, *r = _r;

	return strcmp(l->label, r->label);
}

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

	darray(struct dev_by_label) devs_by_label;
	darray_init(devs_by_label);

	for (i = 0; i < u->nr_devices; i++) {
		struct bch_ioctl_dev_usage *d = u->devs + i;

		if (!d->alive)
			continue;

		char *label_attr = mprintf("dev-%u/label", i);
		char *label = read_file_str(fs.sysfs_fd, label_attr);
		free(label_attr);

		darray_append(devs_by_label,
			(struct dev_by_label) { i, label });
	}

	sort(&darray_item(devs_by_label, 0), darray_size(devs_by_label),
	     sizeof(darray_item(devs_by_label, 0)), dev_by_label_cmp, NULL);

	struct dev_by_label *d;
	darray_foreach(d, devs_by_label)
		print_dev_usage(u->devs + d->idx, d->idx, d->label, units);

	darray_foreach(d, devs_by_label)
		free(d->label);
	darray_free(devs_by_label);

	free(u);
	bcache_fs_close(fs);
}

int cmd_fs_usage(int argc, char *argv[])
{
	enum units units = BYTES;
	char *fs;
	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1)
		switch (opt) {
		case 'h':
			units = HUMAN_READABLE;
			break;
		}
	args_shift(optind);

	if (!argc) {
		print_fs_usage(".", units);
	} else {
		while ((fs = arg_pop()))
			print_fs_usage(fs, units);
	}

	return 0;
}
