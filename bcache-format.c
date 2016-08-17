/*
 * Authors: Kent Overstreet <kent.overstreet@gmail.com>
 *	    Gabriel de Perthuis <g2p.code@gmail.com>
 *	    Jacob Malevich <jam@datera.io>
 *
 * GPLv2
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <uuid/uuid.h>

#include <nih/command.h>
#include <nih/option.h>

#include "ccan/darray/darray.h"

#include "bcache.h"
#include "libbcache.h"
#include "bcache-format.h"
#include "crypto.h"

/* All in units of 512 byte sectors */

static darray(struct dev_opts) cache_devices;

static unsigned block_size, btree_node_size;
static unsigned meta_csum_type = BCH_CSUM_CRC32C;
static unsigned data_csum_type = BCH_CSUM_CRC32C;
static unsigned compression_type = BCH_COMPRESSION_NONE;
static int encrypted;
static unsigned meta_replicas = 1, data_replicas = 1;
static unsigned on_error_action;
static char *label = NULL;
static uuid_le uuid;

/* Device specific options: */
static u64 filesystem_size;
static unsigned bucket_size;
static unsigned tier;
static unsigned replacement_policy;
static int discard;

static int set_cache(NihOption *option, const char *arg)
{
	darray_append(cache_devices, (struct dev_opts) {
		.fd			= dev_open(arg),
		.dev			= strdup(arg),
		.size			= filesystem_size,
		.bucket_size		= bucket_size,
		.tier			= tier,
		.replacement_policy	= replacement_policy,
		.discard		= discard,
	});
	return 0;
}

static int set_uuid(NihOption *option, const char *arg)
{
	if (uuid_parse(arg, uuid.b))
		die("Bad uuid");
	return 0;
}

static int set_block_size(NihOption *option, const char *arg)
{
	block_size = hatoi_validate(arg, "block size");
	return 0;
}

static int set_bucket_sizes(NihOption *option, const char *arg)
{
	bucket_size = hatoi_validate(arg, "bucket size");
	return 0;
}

static int set_btree_node_size(NihOption *option, const char *arg)
{
	btree_node_size = hatoi_validate(arg, "btree node size");
	return 0;
}

static int set_filesystem_size(NihOption *option, const char *arg)
{
	filesystem_size = hatoi(arg) >> 9;
	return 0;
}

static int set_replacement_policy(NihOption *option, const char *arg)
{
	replacement_policy = read_string_list_or_die(arg, replacement_policies,
						     "replacement policy");
	return 0;
}

static int set_csum_type(NihOption *option, const char *arg)
{
	unsigned *csum_type = option->value;

	*csum_type = read_string_list_or_die(arg, csum_types, "checksum type");
	return 0;
}

static int set_compression_type(NihOption *option, const char *arg)
{
	compression_type = read_string_list_or_die(arg, compression_types,
						   "compression type");
	return 0;
}

static int set_on_error_action(NihOption *option, const char *arg)
{
	on_error_action = read_string_list_or_die(arg, error_actions,
						  "error action");
	return 0;
}

static int set_tier(NihOption *option, const char *arg)
{
	tier = strtoul_or_die(arg, CACHE_TIERS, "tier");
	return 0;
}

static int set_meta_replicas(NihOption *option, const char *arg)
{
	meta_replicas = strtoul_or_die(arg, CACHE_SET_META_REPLICAS_WANT_MAX,
				       "meta_replicas");
	return 0;
}

static int set_data_replicas(NihOption *option, const char *arg)
{
	data_replicas = strtoul_or_die(arg, CACHE_SET_DATA_REPLICAS_WANT_MAX,
				       "data_replicas");
	return 0;
}

NihOption opts_format[] = {
//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}

	{ 'C',	"cache",		N_("Format a cache device"),
		NULL, "dev",	NULL,	set_cache },

	{ 'w',	"block",		N_("block size"),
		NULL, "size",	NULL,	set_block_size },
	{ 'n',	"btree_node",		N_("Btree node size, default 256k"),
		NULL, "size",	NULL,	set_btree_node_size },

	{ 0,	"metadata_csum_type",	N_("Checksum type"),
		NULL, "(none|crc32c|crc64)", &meta_csum_type, set_csum_type },
	{ 0,	"data_csum_type",	N_("Checksum type"),
		NULL, "(none|crc32c|crc64)", &data_csum_type, set_csum_type },
	{ 0,	"compression_type",	N_("Compression type"),
		NULL, "(none|gzip)", NULL, set_compression_type },
	{ 0,	"encrypted",		N_("enable encryption"),
		NULL, NULL,		&encrypted,	NULL },

	{ 0,	"meta_replicas",	N_("number of metadata replicas"),
		NULL, "#",	NULL,	set_meta_replicas },
	{ 0,	"data_replicas",	N_("number of data replicas"),
		NULL, "#",	NULL,	set_data_replicas },

	{ 0,	"error_action",		N_("Action to take on filesystem error"),
		NULL, "(continue|readonly|panic)", NULL, set_on_error_action },

	{ 'l',	"label",		N_("label"),
		NULL, "label",	&label, NULL},
	{ 0,	"uuid",			N_("filesystem UUID"),
		NULL, "uuid",	NULL,	set_uuid },

	/* Device specific options: */
	{ 0,	"fs_size",		N_("Size of filesystem on device" ),
		NULL, "size",	NULL,	set_filesystem_size },
	{ 'b',	"bucket",		N_("bucket size"),
		NULL, "size",	NULL,	set_bucket_sizes },
	{ 't',	"tier",			N_("tier of subsequent devices"),
		NULL, "#",	NULL,	set_tier },
	{ 'p',	"cache_replacement_policy", NULL,
		NULL, "(lru|fifo|random)", NULL, set_replacement_policy },
	{ 0,	"discard",		N_("Enable discards"),
		NULL, NULL,		&discard,	NULL },

	NIH_OPTION_LAST
};

int cmd_format(NihCommand *command, char * const *args)
{
	char *passphrase = NULL;

	if (!darray_size(cache_devices))
		die("Please supply a device");

	if (uuid_is_null(uuid.b))
		uuid_generate(uuid.b);

	if (encrypted) {
		char *pass2;

		passphrase = read_passphrase("Enter passphrase: ");
		pass2 = read_passphrase("Enter same passphrase again: ");

		if (strcmp(passphrase, pass2)) {
			memzero_explicit(passphrase, strlen(passphrase));
			memzero_explicit(pass2, strlen(pass2));
			die("Passphrases do not match");
		}

		memzero_explicit(pass2, strlen(pass2));
		free(pass2);
	}

	bcache_format(cache_devices.item, darray_size(cache_devices),
		      block_size,
		      btree_node_size,
		      meta_csum_type,
		      data_csum_type,
		      compression_type,
		      passphrase,
		      meta_replicas,
		      data_replicas,
		      on_error_action,
		      label,
		      uuid);

	if (passphrase) {
		memzero_explicit(passphrase, strlen(passphrase));
		free(passphrase);
	}

	return 0;
}
