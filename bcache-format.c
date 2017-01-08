/*
 * Authors: Kent Overstreet <kent.overstreet@gmail.com>
 *	    Gabriel de Perthuis <g2p.code@gmail.com>
 *	    Jacob Malevich <jam@datera.io>
 *
 * GPLv2
 */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <blkid.h>
#include <uuid/uuid.h>

#include "ccan/darray/darray.h"

#include "bcache-cmds.h"
#include "libbcache.h"

/* Open a block device, do magic blkid stuff: */
static int open_for_format(const char *dev, bool force)
{
	blkid_probe pr;
	const char *fs_type = NULL, *fs_label = NULL;
	size_t fs_type_len, fs_label_len;
	int fd;

	if ((fd = open(dev, O_RDWR|O_EXCL)) == -1)
		die("Can't open dev %s: %s\n", dev, strerror(errno));

	if (force)
		return fd;

	if (!(pr = blkid_new_probe()))
		die("blkid error 1");
	if (blkid_probe_set_device(pr, fd, 0, 0))
		die("blkid error 2");
	if (blkid_probe_enable_partitions(pr, true))
		die("blkid error 3");
	if (blkid_do_fullprobe(pr) < 0)
		die("blkid error 4");

	blkid_probe_lookup_value(pr, "TYPE", &fs_type, &fs_type_len);
	blkid_probe_lookup_value(pr, "LABEL", &fs_label, &fs_label_len);

	if (fs_type) {
		if (fs_label)
			printf("%s contains a %s filesystem labelled '%s'\n",
			       dev, fs_type, fs_label);
		else
			printf("%s contains a %s filesystem\n",
			       dev, fs_type);
		if (!ask_proceed())
			exit(EXIT_FAILURE);
	}

	blkid_free_probe(pr);
	return fd;
}

static void usage(void)
{
	puts("bcache format - create a new bcache filesystem on one or more devices\n"
	     "Usage: bcache format [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -b, --block=size\n"
	     "      --btree_node=size       Btree node size, default 256k\n"
	     "      --metadata_checksum_type=(none|crc32c|crc64)\n"
	     "      --data_checksum_type=(none|crc32c|crc64)\n"
	     "      --compression_type=(none|lz4|gzip)\n"
	     "      --error_action=(continue|readonly|panic)\n"
	     "                              Action to take on filesystem error\n"
	     "      --max_journal_entry_size=size\n"
	     "  -l, --label=label\n"
	     "      --uuid=uuid\n"
	     "  -f, --force\n"
	     "\n"
	     "Device specific options:\n"
	     "      --fs_size=size          Size of filesystem on device\n"
	     "      --bucket=size           bucket size\n"
	     "      --discard               Enable discards\n"
	     "  -t, --tier=#                tier of subsequent devices\n"
	     "\n"
	     "  -h, --help                  display this help and exit\n"
	     "\n"
	     "Device specific options must come before corresponding devices, e.g.\n"
	     "  bcache format --tier 0 /dev/sdb --tier 1 /dev/sdc\n"
	     "\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
	exit(EXIT_SUCCESS);
}

#define OPTS								\
	OPT('b',	block_size,		required_argument)	\
	OPT(0,		btree_node_size,	required_argument)	\
	OPT(0,		metadata_checksum_type,	required_argument)	\
	OPT(0,		data_checksum_type,	required_argument)	\
	OPT(0,		compression_type,	required_argument)	\
	OPT('e',	error_action,		required_argument)	\
	OPT(0,		max_journal_entry_size,	required_argument)	\
	OPT('L',	label,			required_argument)	\
	OPT('U',	uuid,			required_argument)	\
	OPT('f',	force,			no_argument)		\
	OPT(0,		fs_size,		required_argument)	\
	OPT(0,		bucket_size,		required_argument)	\
	OPT('t',	tier,			required_argument)	\
	OPT(0,		discard,		no_argument)		\
	OPT('h',	help,			no_argument)

enum {
	Opt_no_opt = 1,
#define OPT(shortopt, longopt, has_arg)	Opt_##longopt,
	OPTS
#undef OPT
};

static const struct option format_opts[] = {
#define OPT(shortopt, longopt, has_arg)	{				\
		#longopt,  has_arg, NULL, Opt_##longopt			\
	},
	OPTS
#undef OPT
	{ NULL }
};

int cmd_format(int argc, char *argv[])
{
	darray(struct dev_opts) devices;
	struct dev_opts *dev;
	unsigned block_size = 0;
	unsigned btree_node_size = 0;
	unsigned meta_csum_type = BCH_CSUM_CRC32C;
	unsigned data_csum_type = BCH_CSUM_CRC32C;
	unsigned compression_type = BCH_COMPRESSION_NONE;
	unsigned on_error_action = BCH_ON_ERROR_RO;
	char *label = NULL;
	uuid_le uuid;
	bool force = false;

	/* Device specific options: */
	u64 filesystem_size = 0;
	unsigned bucket_size = 0;
	unsigned tier = 0;
	bool discard = false;
	unsigned max_journal_entry_size = 0;
	char *passphrase = NULL;
	int opt;

	darray_init(devices);
	uuid_clear(uuid.b);

	while ((opt = getopt_long(argc, argv,
				  "-b:e:L:U:ft:h",
				  format_opts,
				  NULL)) != -1)
		switch (opt) {
		case Opt_block_size:
		case 'b':
			block_size = hatoi_validate(optarg,
						"block size");
			break;
		case Opt_btree_node_size:
			btree_node_size = hatoi_validate(optarg,
						"btree node size");
			break;
		case Opt_metadata_checksum_type:
			meta_csum_type = read_string_list_or_die(optarg,
						csum_types, "checksum type");
			break;
		case Opt_data_checksum_type:
			data_csum_type = read_string_list_or_die(optarg,
						csum_types, "checksum type");
			break;
		case Opt_compression_type:
			compression_type = read_string_list_or_die(optarg,
						compression_types, "compression type");
			break;
		case Opt_error_action:
		case 'e':
			on_error_action = read_string_list_or_die(optarg,
						error_actions, "error action");
			break;
		case Opt_max_journal_entry_size:
			max_journal_entry_size = hatoi_validate(optarg,
						"journal entry size");
			break;
		case Opt_label:
		case 'L':
			label = strdup(optarg);
			break;
		case Opt_uuid:
		case 'U':
			if (uuid_parse(optarg, uuid.b))
				die("Bad uuid");
			break;
		case Opt_force:
		case 'f':
			force = true;
			break;
		case Opt_fs_size:
			filesystem_size = hatoi(optarg) >> 9;
			break;
		case Opt_bucket_size:
			bucket_size = hatoi_validate(optarg, "bucket size");
			break;
		case Opt_tier:
		case 't':
			tier = strtoul_or_die(optarg, CACHE_TIERS, "tier");
			break;
		case Opt_discard:
			discard = true;
			break;
		case Opt_no_opt:
			darray_append(devices, (struct dev_opts) {
				.path			= strdup(optarg),
				.size			= filesystem_size,
				.bucket_size		= bucket_size,
				.tier			= tier,
				.discard		= discard,
			});
			break;
		case Opt_help:
		case 'h':
			usage();
			break;
		}

	if (!darray_size(devices))
		die("Please supply a device");

	if (uuid_is_null(uuid.b))
		uuid_generate(uuid.b);

	darray_foreach(dev, devices)
		dev->fd = open_for_format(dev->path, force);

	bcache_format(devices.item, darray_size(devices),
		      block_size,
		      btree_node_size,
		      meta_csum_type,
		      data_csum_type,
		      compression_type,
		      1,
		      1,
		      on_error_action,
		      max_journal_entry_size,
		      label,
		      uuid);

	if (passphrase) {
		memzero_explicit(passphrase, strlen(passphrase));
		free(passphrase);
	}

	return 0;
}
