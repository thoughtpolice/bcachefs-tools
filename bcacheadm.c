/*
 * Authors: Kent Overstreet <kmo@daterainc.com>
 *	    Gabriel de Perthuis <g2p.code@gmail.com>
 *	    Jacob Malevich <jam@datera.io>
 *
 * GPLv2
 */

#include <nih/option.h>
#include <nih/command.h>
#include <nih/main.h>
#include <nih/logging.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <blkid.h>
#include <string.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <dirent.h>
#include <bcache.h> //libbcache

#define PACKAGE_NAME "bcacheadm"
#define PACKAGE_VERSION "1.0"
#define PACKAGE_BUGREPORT "bugreport"



/* make-bcache globals */
int bdev = -1;
int devs = 0;
char *cache_devices[MAX_DEVS];
int tier_mapping[MAX_DEVS];
char *backing_devices[MAX_DEVS];
char *backing_dev_labels[MAX_DEVS];
size_t i, nr_backing_devices = 0, nr_cache_devices = 0;
unsigned block_size = 0;
unsigned bucket_sizes[MAX_DEVS];
int num_bucket_sizes = 0;
int writeback = 0, discard = 0, wipe_bcache = 0;
unsigned replication_set = 0, replacement_policy = 0;
uint64_t data_offset = BDEV_DATA_START_DEFAULT;
char *label = NULL;
struct cache_sb *cache_set_sb = NULL;
const char *cache_set_uuid = 0;
const char *csum_type = 0;
char *metadata_replicas = 0;
char *data_replicas = 0;
char *tier = 0;

/* add-dev globals */
char *add_dev_uuid = NULL;

/* rm-dev globals */
bool force_remove = false;

/* Modify globals */
bool modify_list_attrs = false;
static const char *modify_set_uuid = NULL;
static const char *modify_dev_uuid = NULL;

/* query-dev globals */
bool force_csum = false;
bool uuid_only = false;
bool query_brief = false;

/* probe globals */
bool udev = false;

/* list globals */
char *cset_dir = "/sys/fs/bcache";
bool list_devs = false;

/* status globals */
bool status_all = false;

/* capacity globals */
static const char *capacity_uuid = NULL;
bool capacity_devs = false;

/* stats globals */
bool stats_all = false;
bool stats_list = false;
static const char *stats_uuid = NULL;
static const char *stats_dev_uuid = NULL;
static const char *stats_cache_num = NULL;
bool stats_five_min = false;
bool stats_hour = false;
bool stats_day = false;
bool stats_total = false;

/* make-bcache option setters */
static int set_block_size(NihOption *option, const char *arg)
{
	block_size = hatoi_validate(arg, "block size");
	return 0;
}

static int set_cache(NihOption *option, const char *arg)
{
	bdev = 0;
	cache_devices[nr_cache_devices] = strdup(arg);
	if(!tier)
		tier_mapping[nr_cache_devices] = 0;
	else {
		int ntier = atoi(tier);
		if(ntier == 0 || ntier == 1)
			tier_mapping[nr_cache_devices] = ntier;
		else
			printf("Invalid tier\n");
	}

	devs++;
	nr_cache_devices++;

	return 0;
}

static int set_bdev(NihOption *option, const char *arg)
{
	bdev = 1;

	if(label)
		backing_dev_labels[nr_backing_devices] = strdup(label);

	backing_devices[nr_backing_devices] = strdup(arg);

	nr_backing_devices++;
	devs++;

	return 0;
}

static int set_bucket_sizes(NihOption *option, const char *arg)
{
	bucket_sizes[num_bucket_sizes]=hatoi_validate(arg, "bucket size");
	num_bucket_sizes++;
	return 0;
}

/* probe setters */
static int set_udev(NihOption *option, const char *arg)
{
	if (strcmp("udev", arg)) {
		printf("Invalid output format %s\n", arg);
		exit(EXIT_FAILURE);
	}
	udev = true;
	return 0;
}


/* options */
static NihOption make_bcache_options[] = {
//	{int shortoption, char* longoption, char* help, NihOptionGroup, char* argname, void *value, NihOptionSetter}
	{'C', "cache",	N_("Format a cache device"), NULL, "dev", NULL, set_cache},
	{'B', "bdev",	N_("Format a backing device"), NULL, "dev", NULL, set_bdev},
	{'l', "label",	N_("label"), NULL, "label", &label, NULL},
	//Only one bucket_size supported until a list of bucket sizes is parsed correctly
	{'b', "bucket",	N_("bucket size"), NULL, "size", NULL, set_bucket_sizes},
	//Does the default setter automatically convert strings to an int?
	{'w', "block",	N_("block size (hard sector size of SSD, often 2k"), NULL,"size", NULL, set_block_size},
	{'t', "tier",	N_("tier of subsequent devices"), NULL,"#", &tier, NULL},
	{'p', "cache_replacement_policy", N_("one of (lru|fifo|random)"), NULL,"policy", &replacement_policy, NULL},
	{'o', "data_offset", N_("data offset in sectors"), NULL,"offset", &data_offset, NULL},

	{0, "cset-uuid",	N_("UUID for the cache set"),		NULL,	"uuid", &cache_set_uuid, NULL},
	{0, "csum-type",	N_("One of (none|crc32c|crc64)"),		NULL,	"type", &csum_type, NULL },
	{0, "replication-set",N_("replication set of subsequent devices"),	NULL,	NULL, &replication_set, NULL },
	{0, "meta-replicas",N_("number of metadata replicas"),		NULL,	"#", &metadata_replicas, NULL},
	{0, "data-replicas",N_("number of data replicas"),		NULL,	"#", &data_replicas, NULL },

	{0, "wipe-bcache",	N_("destroy existing bcache data if present"),		NULL, NULL, &wipe_bcache, NULL},
	{0, "discard",		N_("enable discards"),		NULL, NULL, &discard,		NULL},
	{0, "writeback",	N_("enable writeback"),		NULL, NULL, &writeback, 	NULL},

	NIH_OPTION_LAST
};

static NihOption probe_bcache_options[] = {
	{'o', "udev", N_("udev"), NULL, NULL, NULL, set_udev},
	NIH_OPTION_LAST
};

static NihOption bcache_register_options[] = {
	NIH_OPTION_LAST
};

static NihOption bcache_unregister_options[] = {
	NIH_OPTION_LAST
};

static NihOption bcache_add_device_options[] = {
	{'u', "set", N_("cacheset uuid"), NULL, "UUID", &add_dev_uuid, NULL},
	NIH_OPTION_LAST
};

static NihOption bcache_rm_device_options[] = {
	{'f', "force", N_("force cache removal"), NULL, NULL, &force_remove, NULL},
	NIH_OPTION_LAST
};

static NihOption bcache_modify_options[] = {
	{'l', "list", N_("list attributes"), NULL, NULL, &modify_list_attrs, NULL},
	{'u', "set", N_("cacheset uuid"), NULL, "UUID", &modify_set_uuid, NULL},
	{'d', "dev", N_("device uuid"), NULL, "UUID", &modify_dev_uuid, NULL},
	NIH_OPTION_LAST
};

static NihOption query_devs_options[] = {
	{'f', "force_csum", N_("force_csum"), NULL, NULL, &force_csum, NULL},
	{'u', "uuid-only", N_("only print out the uuid for the devices, not the whole superblock"), NULL, NULL, &uuid_only, NULL},
	{'b', "brief", N_("only print out the cluster,server,and disk uuids"), NULL, NULL, &query_brief, NULL},
	NIH_OPTION_LAST
};

static NihOption list_cachesets_options[] = {
	{'d', "dir", N_("directory"), NULL, NULL, &cset_dir, NULL},
	{0, "list-devs", N_("list all devices in the cache sets as well"), NULL, NULL, &list_devs, NULL},
	NIH_OPTION_LAST
};

static NihOption status_options[] = {
	{'a', "all", N_("all"), NULL, NULL, &status_all, NULL},
	NIH_OPTION_LAST
};

static NihOption capacity_options[] = {
	{'u', "set", N_("cache_set UUID"), NULL, "UUID", &capacity_uuid, NULL},
	{'d', "devs", N_("dev UUID"), NULL, NULL, &capacity_devs, NULL},
	NIH_OPTION_LAST
};

static NihOption stats_options[] = {
	{'a', "all", N_("all"), NULL, NULL, &stats_all, NULL},
	{'l', "list", N_("list"), NULL, NULL, &stats_list, NULL},
	{'u', "set", N_("cache_set UUID"), NULL, "UUID", &stats_uuid, NULL},
	{'d', "dev", N_("dev UUID"), NULL, "UUID", &stats_dev_uuid, NULL},
	{'c', "cache", N_("cache number (starts from 0)"), NULL, "CACHE#", &stats_cache_num, NULL},
	{0, "five-min-stats", N_("stats accumulated in last 5 minutes"), NULL, NULL, &stats_five_min, NULL},
	{0, "hour-stats", N_("stats accumulated in last hour"), NULL, NULL, &stats_hour, NULL},
	{0, "day-stats", N_("stats accumulated in last day"), NULL, NULL, &stats_day, NULL},
	{0, "total-stats", N_("stats accumulated in total"), NULL, NULL, &stats_total, NULL},
	NIH_OPTION_LAST
};

static NihOption options[] = {
	NIH_OPTION_LAST
};


/* commands */
int make_bcache(NihCommand *command, char *const *args)
{
	int cache_dev_fd[devs];

	int backing_dev_fd[devs];

	cache_set_sb = calloc(1, sizeof(*cache_set_sb) +
				     sizeof(struct cache_member) * devs);

	if (cache_set_uuid) {
		if(uuid_parse(cache_set_uuid, cache_set_sb->set_uuid.b)) {
			fprintf(stderr, "Bad uuid\n");
			return -1;
		}
	} else {
		uuid_generate(cache_set_sb->set_uuid.b);
	}

	if (label) 
		memcpy(cache_set_sb->label, label, sizeof(cache_set_sb->label));

	if (csum_type) {
		SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb,
				read_string_list_or_die(csum_type, csum_types,
					"csum type"));
	} else {
		SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb, BCH_CSUM_CRC32C);
	}

	if (metadata_replicas) {
		SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb,
				strtoul_or_die(metadata_replicas,
					CACHE_SET_META_REPLICAS_WANT_MAX,
					"meta replicas"));
	} else {
		SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb, 1);
	}

	if (data_replicas) {
		SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb,
			strtoul_or_die(data_replicas,
				CACHE_SET_DATA_REPLICAS_WANT_MAX,
				"data replicas"));
	} else {
		SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb, 1);
	}

	if (bdev == -1) {
		fprintf(stderr, "Please specify -C or -B\n");
		exit(EXIT_FAILURE);
	}

	if(!bucket_sizes[0]) bucket_sizes[0] = 1024;

	for(i = 0; i < nr_cache_devices; i++)
		next_cache_device(cache_set_sb,
				  replication_set,
				  tier_mapping[i],
				  replacement_policy,
				  discard);

	if (!cache_set_sb->nr_in_set && !nr_backing_devices) {
		fprintf(stderr, "Please supply a device\n");
		exit(EXIT_FAILURE);
	}

	i = 0;
	do {
		if (bucket_sizes[i] < block_size) {
			fprintf(stderr,
			"Bucket size cannot be smaller than block size\n");
			exit(EXIT_FAILURE);
		}
		i++;
	} while (i < num_bucket_sizes);

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

	write_cache_sbs(cache_dev_fd, cache_set_sb, block_size,
					bucket_sizes, num_bucket_sizes);

	for (i = 0; i < nr_backing_devices; i++)
		write_backingdev_sb(backing_dev_fd[i],
				    block_size, bucket_sizes,
				    writeback, data_offset,
				    backing_dev_labels[i],
				    cache_set_sb->set_uuid);


	return 0;
}

int probe_bcache(NihCommand *command, char *const *args)
{
	int i;
	char *err = NULL;

	for (i = 0; args[i] != NULL; i++) {
		err = probe(args[i], udev);
		if(err) {
			printf("probe_bcache error: %s\n", err);
			return -1;
		}
	}

	return 0;
}

int bcache_register(NihCommand *command, char *const *args)
{
	char *err = NULL;

	err = register_bcache(args);
	if (err) {
		printf("bcache_register error: %s\n", err);
		return -1;
	}

	return 0;
}

int bcache_unregister(NihCommand *command, char *const *args)
{
	char *err = NULL;

	err = unregister_bcache(args);
	if (err) {
		printf("bcache_unregister error: %s\n", err);
		return -1;
	}

	return 0;
}

int bcache_add_devices(NihCommand *command, char *const *args)
{
	char *err;

	if (!add_dev_uuid) {
		printf("Must specify a cacheset uuid to add the disk to\n");
		return -1;
	}

	err = add_devices(args, add_dev_uuid);
	if (err) {
		printf("bcache_add_devices error: %s\n", err);
		return -1;
	}

	return 0;
}

int bcache_rm_device(NihCommand *command, char *const *args)
{
	char *err;

	err = remove_device(args[0], force_remove);
	if (err) {
		printf("bcache_rm_devices error: %s\n", err);
		return -1;
	}

	return 0;
}

int bcache_modify(NihCommand *command, char *const *args)
{
	char *err;
	char path[MAX_PATH];
	DIR *path_dir;
	struct stat cache_stat;
	char *attr = args[0];
	char *val = NULL;
	int fd = -1;

	if (modify_list_attrs) {
		sysfs_attr_list();
		return 0;
	}

	if (!modify_set_uuid) {
		printf("Must provide a cacheset uuid\n");
		return -1;
	}

	snprintf(path, MAX_PATH, "%s/%s", cset_dir, modify_set_uuid);

	if(!attr) {
		printf("Must provide the name of an attribute to modify\n");
		goto err;
	}

	enum sysfs_attr type = sysfs_attr_type(attr);

	if (type == -1)
		goto err;
	else if(type == INTERNAL_ATTR)
		strcat(path, "/internal");
	else if(type == CACHE_ATTR) {
		if(modify_dev_uuid) {
			/* searches all cache# for a matching uuid,
			 * path gets modified to the correct cache path */
			char subdir[10] = "/cache";
			err = find_matching_uuid(path, subdir,
					modify_dev_uuid);
			if (err) {
				printf("Failed to find "
					"matching dev %s\n", err);
				goto err;
			} else {
				strcat(path, subdir);
			}
		} else {
			printf("Must provide a device uuid\n");
		}
	}
	/* SET_ATTRs are just in the current dir */

	strcat(path, "/");
	strcat(path, attr);

	val = args[1];
	if (!val) {
		printf("Must provide a value to change the attribute to\n");
		goto err;
	}

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		printf("Unable to open modify attr with path %s\n", path);
		goto err;
	}

	write(fd, val, strlen(val));

err:
	if(fd)
		close(fd);
	return 0;
}

int bcache_list_cachesets(NihCommand *command, char *const *args)
{
	char *err = NULL;
	err = list_cachesets(cset_dir, list_devs);
	if (err) {
		printf("bcache_list_cachesets error :%s\n", err);
		return -1;
	}

	return 0;
}

int bcache_query_devs(NihCommand *command, char *const *args)
{
	int i;

	if (query_brief)
		printf("%-10s%-40s%-40s%-40s\n", "dev name", "disk uuid",
				"server uuid", "cluster uuid");

	for (i = 0; args[i] != NULL; i++) {
		char dev_uuid[40];
		struct cache_sb *sb = query_dev(args[i], force_csum,
				!query_brief, uuid_only, dev_uuid);

		if (!sb) {
			printf("error opening the superblock for %s\n",
					args[i]);
			return -1;
		}

		if (uuid_only) {
			printf("%s\n", dev_uuid);
		} else if (query_brief) {
			char set_uuid_str[40], dev_uuid_str[40];
			char *clus_uuid = (char *)sb->label;

			uuid_unparse(sb->set_uuid.b, set_uuid_str);
			uuid_unparse(sb->uuid.b, dev_uuid_str);
			if (!strcmp(clus_uuid, ""))
				clus_uuid = "None";

			printf("%-10s%-40s%-40s%-40s\n", args[i],
					dev_uuid_str,
					set_uuid_str,
					clus_uuid);
		}
		free(sb);
	}

	return 0;
}

int bcache_status(NihCommand *command, char *const *args)
{
	int i, seq, nr_in_set = 0;
	struct cache_sb *seq_sb = NULL;

	for (i = 0; args[i] != NULL; i++) {
		struct cache_sb *sb = query_dev(args[i], false, false,
				false, NULL);

		if(!sb) {
			printf("Unable to open superblock, bad path\n");
			return -1;
		}

		if(!seq_sb || sb->seq > seq) {
			seq = sb->seq;
			seq_sb = sb;
			nr_in_set = sb->nr_in_set;
		} else
			free(sb);
	}

	if(!seq_sb)
		printf("Unable to find a superblock\n");
	else
		printf("%-50s%-15s%-4s\n", "uuid", "state", "tier");

	for (i = 0; i < seq_sb->nr_in_set; i++) {
		char uuid_str[40];
		struct cache_member *m = ((struct cache_member *) seq_sb->d) + i;

		uuid_unparse(m->uuid.b, uuid_str);

		printf("%-50s%-15s%-4llu\n", uuid_str,
				cache_state[CACHE_STATE(m)],
				CACHE_TIER(m));
	}

	if(seq_sb)
		free(seq_sb);

	return 0;
}

int bcache_capacity(NihCommand *command, char *const *args)
{
	char *err = "Must provide a cacheset uuid";
	if(!capacity_uuid)
		goto err;

	err = bcache_get_capacity(cset_dir, capacity_uuid, capacity_devs);
	if (err)
		goto err;

	return 0;

err:
	printf("bcache_capacity failed with error: %s\n", err);
	return -1;

}

static char *stats_subdir(char* stats_dir)
{
	char tmp[50] = "/";
	char *err = NULL;
	if(stats_dev_uuid) {
		strcat(tmp, "cache");
		err = find_matching_uuid(stats_dir, tmp, stats_dev_uuid);
		if(err)
			goto err;
	} else if(stats_cache_num) {
		strcat(tmp, "cache");
		strcat(tmp, stats_cache_num);
	} else if (stats_five_min)
		strcat(tmp, "stats_five_minute");
	else if (stats_hour)
		strcat(tmp, "stats_hour");
	else if (stats_day)
		strcat(tmp, "stats_day");
	else if (stats_total)
		strcat(tmp, "stats_total");
	else
		return err;

	strcat(stats_dir, tmp);

err:
	return err;
}

int bcache_stats(NihCommand *command, char *const *args)
{
	int i;
	char stats_dir[MAX_PATH];
	DIR *dir = NULL;
	struct dirent *ent = NULL;
	char *err = NULL;
	char buf[MAX_PATH];

	if (stats_uuid) {
		snprintf(stats_dir, MAX_PATH, "%s/%s", cset_dir, stats_uuid);
		err = stats_subdir(stats_dir);
		if(err)
			goto err;

		dir = opendir(stats_dir);
		if (!dir) {
			err = "Failed to open dir";
			goto err;
		}
	} else {
		err = "Must provide a cacheset uuid";
		goto err;
	}

	if(stats_list || stats_all) {
		while ((ent = readdir(dir)) != NULL) {
			err = read_stat_dir(dir, stats_dir, ent->d_name, buf);
			if (err)
				goto err;
			if(stats_list)
				printf("%s\n", ent->d_name);
			if(stats_all)
				printf("\t%s\n", buf);
		}
	}

	for (i = 0; args[i] != NULL; i++) {
		err = read_stat_dir(dir, stats_dir, args[i], buf);
		if (err)
			goto err;
		printf("%s\n", buf);
	}

	closedir(dir);
	return 0;

err:
	closedir(dir);
	printf("bcache_stats error: %s\n", err);
	return -1;
}

static NihCommand commands[] = {
	{"format", N_("format <list of drives>"),
		  "Format one or a list of devices with bcache datastructures."
		  " You need to do this before you create a volume",
		  N_("format drive[s] with bcache"),
		  NULL, make_bcache_options, make_bcache},
	{"probe", N_("probe <list of devices>"),
		  "Does a blkid_probe on a device",
		  N_("Does a blkid_probe on a device"),
		  NULL, probe_bcache_options, probe_bcache},
	{"register", N_("register <list of devices>"),
		     "Registers a list of devices",
		     N_("Registers a list of devices"),
		     NULL, bcache_register_options, bcache_register},
	{"unregister", N_("unregister <list of devices>"),
		     "Unregisters a list of devices",
		     N_("Unregisters a list of devices"),
		     NULL, bcache_unregister_options, bcache_unregister},
	{"add-devs", N_("add-devs --set=UUID --tier=# <list of devices>"),
		"Adds a list of devices to a cacheset",
		N_("Adds a list of devices to a cacheset"),
		NULL, bcache_add_device_options, bcache_add_devices},
	{"rm-dev", N_("rm-dev <dev>"),
		"Removes a device from its cacheset",
		N_("Removes a device from its cacheset"),
		NULL, bcache_rm_device_options, bcache_rm_device},
	{"modify", N_("modify --set=UUID (dev=UUID) name value"),
		"Modifies attributes related to the cacheset",
		N_("Modifies attributes related to the cacheset"),
		NULL, bcache_modify_options, bcache_modify},
	{"list-cachesets", N_("list-cachesets"),
			   "Lists cachesets in /sys/fs/bcache",
			   N_("Lists cachesets in /sys/fs/bcache"),
			   NULL, list_cachesets_options, bcache_list_cachesets},
	{"query-devs", N_("query <list of devices>"),
		       "Gives info about the superblock of a list of devices",
		       N_("show superblock on each of the listed drive"),
		       NULL, query_devs_options, bcache_query_devs},
	{"status", N_("status <list of devices>"),
		   "Finds the status of the most up to date superblock",
		   N_("Finds the status of the most up to date superblock"),
		   NULL, status_options, bcache_status},
	{"capacity", N_("capacity --set=UUID"),
		"Shows the capacity of the cacheset",
		N_("Shows the capacity of the cacheset"),
		NULL, capacity_options, bcache_capacity},
	{"stats", N_("stats <list of devices>"),
		  "List various bcache statistics",
		  N_("List various bcache statistics"),
		  NULL, stats_options, bcache_stats},
	NIH_COMMAND_LAST
};


int main(int argc, char *argv[])
{
	int ret = 0;
	nih_main_init (argv[0]);

	nih_option_set_synopsis (_("Manage bcache devices"));
	nih_option_set_help (
			_("Helps you manage bcache devices"));

	ret = nih_command_parser (NULL, argc, argv, options, commands);
	if (ret < 0)
		exit (1);

	nih_signal_reset();

	return 0;
}
