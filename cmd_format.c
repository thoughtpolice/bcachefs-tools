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

#include "cmds.h"
#include "libbcache.h"
#include "crypto.h"
#include "opts.h"
#include "util.h"

/* Open a block device, do magic blkid stuff: */
static int open_for_format(const char *dev, bool force)
{
	blkid_probe pr;
	const char *fs_type = NULL, *fs_label = NULL;
	size_t fs_type_len, fs_label_len;

	int fd = xopen(dev, O_RDWR|O_EXCL);

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
		fputs("Proceed anyway?", stdout);
		if (!ask_yn())
			exit(EXIT_FAILURE);
	}

	blkid_free_probe(pr);
	return fd;
}

#define OPTS									\
t("bcache format - create a new bcache filesystem on one or more devices")	\
t("Usage: bcache format [OPTION]... <devices>")					\
t("")										\
x('b',	block_size,		"size",			NULL)			\
x(0,	btree_node_size,	"size",			"Default 256k")		\
x(0,	metadata_checksum_type,	"(none|crc32c|crc64)",	NULL)			\
x(0,	data_checksum_type,	"(none|crc32c|crc64)",	NULL)			\
x(0,	compression_type,	"(none|lz4|gzip)",	NULL)			\
x(0,	encrypted,		NULL,			"Enable whole filesystem encryption (chacha20/poly1305)")\
x(0,	no_passphrase,		NULL,			"Don't encrypt master encryption key")\
x('e',	error_action,		"(continue|readonly|panic)", NULL)		\
x(0,	max_journal_entry_size,	"size",			NULL)			\
x('L',	label,			"label",		NULL)			\
x('U',	uuid,			"uuid",			NULL)			\
x('f',	force,			NULL,			NULL)			\
t("")										\
t("Device specific options:")							\
x(0,	fs_size,		"size",			"Size of filesystem on device")\
x(0,	bucket_size,		"size",			"Bucket size")		\
x('t',	tier,			"#",			"Higher tier indicates slower devices")\
x(0,	discard,		NULL,			NULL)			\
t("Device specific options must come before corresponding devices, e.g.")	\
t("  bcache format --tier 0 /dev/sdb --tier 1 /dev/sdc")			\
t("")										\
x('h',	help,			NULL,			"display this help and exit")

static void usage(void)
{
#define t(text)				puts(text "\n")
#define x(shortopt, longopt, arg, help) do {				\
	OPTS
#undef x
#undef t

	puts("bcache format - create a new bcache filesystem on one or more devices\n"
	     "Usage: bcache format [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -b, --block=size\n"
	     "      --btree_node=size       Btree node size, default 256k\n"
	     "      --metadata_checksum_type=(none|crc32c|crc64)\n"
	     "      --data_checksum_type=(none|crc32c|crc64)\n"
	     "      --compression_type=(none|lz4|gzip)\n"
	     "      --encrypted             Enable whole filesystem encryption (chacha20/poly1305)\n"
	     "      --no_passphrase         Don't encrypt master encryption key\n"
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
}

enum {
	Opt_no_opt = 1,
#define t(text)
#define x(shortopt, longopt, arg, help)	Opt_##longopt,
	OPTS
#undef x
#undef t
};

static const struct option format_opts[] = {
#define t(text)
#define x(shortopt, longopt, arg, help)	{				\
	.name		= #longopt,					\
	.has_arg	= arg ? required_argument : no_argument,	\
	.flag		= NULL,						\
	.val		= Opt_##longopt,				\
},
	OPTS
#undef x
#undef t
	{ NULL }
};

static unsigned hatoi_validate(const char *s, const char *msg)
{
	u64 v;

	if (bch_strtoull_h(s, &v))
		die("bad %s %s", msg, s);

	if (v & (v - 1))
		die("%s must be a power of two", msg);

	v /= 512;

	if (v > USHRT_MAX)
		die("%s too large\n", msg);

	if (!v)
		die("%s too small\n", msg);

	return v;
}

int cmd_format(int argc, char *argv[])
{
	darray(struct dev_opts) devices;
	struct format_opts opts = format_opts_default();
	struct dev_opts dev_opts = { 0 }, *dev;
	bool force = false, no_passphrase = false;
	int opt;

	darray_init(devices);

	while ((opt = getopt_long(argc, argv,
				  "-b:e:L:U:ft:h",
				  format_opts,
				  NULL)) != -1)
		switch (opt) {
		case Opt_block_size:
		case 'b':
			opts.block_size =
				hatoi_validate(optarg, "block size");
			break;
		case Opt_btree_node_size:
			opts.btree_node_size =
				hatoi_validate(optarg, "btree node size");
			break;
		case Opt_metadata_checksum_type:
			opts.meta_csum_type =
				read_string_list_or_die(optarg,
						bch_csum_types, "checksum type");
			break;
		case Opt_data_checksum_type:
			opts.data_csum_type =
				read_string_list_or_die(optarg,
						bch_csum_types, "checksum type");
			break;
		case Opt_compression_type:
			opts.compression_type =
				read_string_list_or_die(optarg,
						bch_compression_types,
						"compression type");
			break;
		case Opt_encrypted:
			opts.encrypted = true;
			break;
		case Opt_no_passphrase:
			no_passphrase = true;
			break;
		case Opt_error_action:
		case 'e':
			opts.on_error_action =
				read_string_list_or_die(optarg,
						bch_error_actions, "error action");
			break;
		case Opt_max_journal_entry_size:
			opts.max_journal_entry_size =
				hatoi_validate(optarg, "journal entry size");
			break;
		case Opt_label:
		case 'L':
			opts.label = strdup(optarg);
			break;
		case Opt_uuid:
		case 'U':
			if (uuid_parse(optarg, opts.uuid.b))
				die("Bad uuid");
			break;
		case Opt_force:
		case 'f':
			force = true;
			break;
		case Opt_fs_size:
			if (bch_strtoull_h(optarg, &dev_opts.size))
				die("invalid filesystem size");

			dev_opts.size >>= 9;
			break;
		case Opt_bucket_size:
			dev_opts.bucket_size =
				hatoi_validate(optarg, "bucket size");
			break;
		case Opt_tier:
		case 't':
			if (kstrtouint(optarg, 10, &dev_opts.tier) ||
			    dev_opts.tier >= BCH_TIER_MAX)
				die("invalid tier");
			break;
		case Opt_discard:
			dev_opts.discard = true;
			break;
		case Opt_no_opt:
			dev_opts.path = strdup(optarg);
			darray_append(devices, dev_opts);
			dev_opts.size = 0;
			break;
		case Opt_help:
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		}

	if (!darray_size(devices))
		die("Please supply a device");

	if (opts.encrypted && !no_passphrase) {
		opts.passphrase = read_passphrase("Enter passphrase: ");

		if (isatty(STDIN_FILENO)) {
			char *pass2 =
				read_passphrase("Enter same passphrase again: ");

			if (strcmp(opts.passphrase, pass2)) {
				memzero_explicit(opts.passphrase,
						 strlen(opts.passphrase));
				memzero_explicit(pass2, strlen(pass2));
				die("Passphrases do not match");
			}

			memzero_explicit(pass2, strlen(pass2));
			free(pass2);
		}
	}

	darray_foreach(dev, devices)
		dev->fd = open_for_format(dev->path, force);

	struct bch_sb *sb =
		bcache_format(opts, devices.item, darray_size(devices));
	bcache_super_print(sb, HUMAN_READABLE);
	free(sb);

	if (opts.passphrase) {
		memzero_explicit(opts.passphrase, strlen(opts.passphrase));
		free(opts.passphrase);
	}

	return 0;
}
