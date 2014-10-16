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
#include <bcache.h> //libbcache

#define PACKAGE_NAME "bcacheadm"
#define PACKAGE_VERSION "1.0"
#define PACKAGE_BUGREPORT "bugreport"

//What is the actual max?
#define MAX_DEVS 64



/* bcacheadm globals */
enum exit {
	EXIT_OK = 0,		/* Ok */
	EXIT_ERROR = 1,		/* General/OS error */
	EXIT_SHELL = 2,		/* Start maintenance shell */
	EXIT_SHELL_REBOOT = 3,	/* Start maintenance shell, reboot when done */
	EXIT_REBOOT = 4,	/* System must reboot */
};


/* make-bcache globals */
int bdev = -1;
int devs = 0;
const char *cache_devices[MAX_DEVS];
const char *backing_devices[MAX_DEVS];
const char *backing_dev_labels[MAX_DEVS];
size_t i, nr_backing_devices = 0;
unsigned block_size = 0;
unsigned bucket_sizes[MAX_DEVS];
int num_bucket_sizes = 0;
int writeback = 0, discard = 0, wipe_bcache = 0;
unsigned replication_set = 0, tier = 0, replacement_policy = 0;
uint64_t data_offset = BDEV_DATA_START_DEFAULT;
char *label = NULL;
struct cache_sb *cache_set_sb;
enum long_opts {
	CACHE_SET_UUID = 256,
	CSUM_TYPE,
	REPLICATION_SET,
	META_REPLICAS,
	DATA_REPLICAS,
};


/* super-show globals */
bool force_csum = false;

/* probe globals */
bool udev = false;

/* list globals */
char *cset_dir = "/sys/fs/bcache";

/* make-bcache option setters */
static int set_CACHE_SET_UUID(NihOption *option, const char *arg)
{
	if(uuid_parse(arg, cache_set_sb->set_uuid.b)){
		fprintf(stderr, "Bad uuid\n");
		return -1;
	}
	return 0;
}
static int set_CSUM_TYPE(NihOption *option, const char *arg)
{
	SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb,
			read_string_list_or_die(arg, csum_types,
				"csum type"));
	return 0;
}
static int set_REPLICATION_SET(NihOption *option, const char *arg)
{
	replication_set = strtoul_or_die(arg,
						CACHE_REPLICATION_SET_MAX,
						"replication set");
	return 0;
}
static int set_META_REPLICAS(NihOption *option, const char *arg)
{
	SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb,
			strtoul_or_die(arg,
						CACHE_SET_META_REPLICAS_WANT_MAX,
						"meta replicas"));
	return 0;
}
static int set_DATA_REPLICAS(NihOption *option, const char *arg)
{
	SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb,
				strtoul_or_die(arg,
						CACHE_SET_DATA_REPLICAS_WANT_MAX,
						"data replicas"));
	return 0;
}
static int set_cache(NihOption *option, const char *arg)
{
	bdev=0;
	cache_devices[cache_set_sb->nr_in_set] = arg;
	next_cache_device(cache_set_sb,
			  replication_set,
			  tier,
			  replacement_policy,
			  discard);
	devs++;
	return 0;
}
static int set_bdev(NihOption *option, const char *arg)
{
	bdev=1;
	backing_dev_labels[nr_backing_devices]=label;
	backing_devices[nr_backing_devices++]=arg;
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
	{'C', "cache",	N_("Format a cache device"), NULL, NULL, NULL, set_cache},
	{'B', "bdev",	N_("Format a backing device"), NULL, NULL, NULL, set_bdev},
	{'l', "label",	N_("label"), NULL, NULL, &label, NULL},
	//Only one bucket_size supported until a list of bucket sizes is parsed correctly
	{'b', "bucket",	N_("bucket size"), NULL, NULL, NULL, set_bucket_sizes},
	//Does the default setter automatically convert strings to an int?
	{'w', "block",	N_("block size (hard sector size of SSD, often 2k"), NULL,NULL,	&block_size, NULL},
	{'t', "tier",	N_("tier of subsequent devices"), NULL,NULL, &tier, NULL},
	{'p', "cache_replacement_policy", N_("one of (lru|fifo|random)"), NULL,NULL, &replacement_policy, NULL},
	{'o', "data_offset", N_("data offset in sectors"), NULL,NULL, &data_offset, NULL},

	{0, "cset-uuid",	N_("UUID for the cache set"),		NULL,	NULL, NULL, set_CACHE_SET_UUID },
	{0, "csum-type",	N_("One of (none|crc32c|crc64)"),		NULL,	NULL, NULL, set_CSUM_TYPE },
	{0, "replication-set",N_("replication set of subsequent devices"),	NULL,	NULL, NULL, set_REPLICATION_SET },
	{0, "meta-replicas",N_("number of metadata replicas"),		NULL,	NULL, NULL, set_META_REPLICAS},
	{0, "data-replicas",N_("number of data replicas"),		NULL,	NULL, NULL, set_DATA_REPLICAS },

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

static NihOption query_devs_options[] = {
	{'f', "force_csum", N_("force_csum"), NULL, NULL, &force_csum, NULL},
	NIH_OPTION_LAST
};

static NihOption list_cachesets_options[] = {
	{'d', "dir", N_("directory"), NULL, NULL, &cset_dir, NULL},
	NIH_OPTION_LAST
};

static NihOption options[] = {
	NIH_OPTION_LAST
};


/* commands */
int make_bcache (NihCommand *command, char *const *args)
{
	int cache_dev_fd[devs];

	int backing_dev_fd[devs];

	cache_set_sb = calloc(1, sizeof(*cache_set_sb) +
				     sizeof(struct cache_member) * devs);

	uuid_generate(cache_set_sb->set_uuid.b);
	SET_CACHE_PREFERRED_CSUM_TYPE(cache_set_sb, BCH_CSUM_CRC32C);
	SET_CACHE_SET_META_REPLICAS_WANT(cache_set_sb, 1);
	SET_CACHE_SET_DATA_REPLICAS_WANT(cache_set_sb, 1);

	if(!bucket_sizes[0]) bucket_sizes[0] = 1024;

	if (bdev == -1) {
		fprintf(stderr, "Please specify -C or -B\n");
		exit(EXIT_FAILURE);
	}

	if (!cache_set_sb->nr_in_set && !nr_backing_devices) {
		fprintf(stderr, "Please supply a device\n");
		exit(EXIT_FAILURE);
	}

	i = 0;
	do {
		if (bucket_sizes[i] < block_size) {
			fprintf(stderr, "Bucket size cannot be smaller than block size\n");
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

int probe_bcache (NihCommand *command, char *const *args)
{
	int i;

	for (i = 0; args[i] != NULL; i++) {
		probe(args[i], udev);
	}

	return 0;
}

int bcache_register (NihCommand *command, char *const *args)
{
	int ret;
	char *arg_list = parse_array_to_list(args);

	if(arg_list) {
		ret = register_bcache(arg_list);
		free(arg_list);
	}

	return ret;
}

int bcache_list_cachesets (NihCommand *command, char *const *args)
{
	return list_cachesets(cset_dir);
}

int bcache_query_devs (NihCommand *command, char *const *args)
{
	int i;

	for (i = 0; args[i]!=NULL; i++) {
		query_dev(args[i], false);
	}
}

static NihCommand commands[] = {
	{"format", N_("format <list of drives>"), "Format one or a list of devices with bcache datastructures. You need to do this before you create a volume",  N_("format drive[s] with bcache"), NULL, make_bcache_options, make_bcache},
	{"probe", N_("probe <list of devices>"), "Does a blkid_probe on a device", N_("Does a blkid_probe on a device"), NULL, probe_bcache_options, probe_bcache},
	{"register", N_("register <list of devices>"), "Registers a list of devices", N_("Registers a list of devices"), NULL, bcache_register_options, bcache_register},
	{"list-cachesets", N_("list-cachesets"), "Lists cachesets in /sys/fs/bcache", N_("Lists cachesets in /sys/fs/bcache"), NULL, list_cachesets_options, bcache_list_cachesets},
	{"query-devs", N_("query <list of devices>"), "Gives info about the superblock of a list of devices", N_("show superblock on each of the listed drive"), NULL, query_devs_options, bcache_query_devs},
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
}
