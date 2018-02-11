#include <errno.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "cmds.h"
#include "libbcachefs/checksum.h"
#include "crypto.h"
#include "libbcachefs.h"

static void unlock_usage(void)
{
	puts("bcachefs unlock - unlock an encrypted filesystem so it can be mounted\n"
	     "Usage: bcachefs unlock [OPTION] device\n"
	     "\n"
	     "Options:\n"
	     "  -c                     Check if a device is encrypted\n"
	     "  -h                     Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

int cmd_unlock(int argc, char *argv[])
{
	bool check = false;
	int opt;

	while ((opt = getopt(argc, argv, "ch")) != -1)
		switch (opt) {
		case 'c':
			check = true;
			break;
		case 'h':
			unlock_usage();
			exit(EXIT_SUCCESS);
		}
	args_shift(optind);

	char *dev = arg_pop();
	if (!dev)
		die("Please supply a device");

	if (argc)
		die("Too many arguments");

	struct bch_opts opts = bch2_opts_empty();

	opt_set(opts, noexcl, true);
	opt_set(opts, nochanges, true);

	struct bch_sb_handle sb;
	int ret = bch2_read_super(dev, &opts, &sb);
	if (ret)
		die("Error opening %s: %s", dev, strerror(-ret));

	if (!bch2_sb_is_encrypted(sb.sb))
		die("%s is not encrypted", dev);

	if (check)
		exit(EXIT_SUCCESS);

	char *passphrase = read_passphrase("Enter passphrase: ");

	bch2_add_key(sb.sb, passphrase);

	bch2_free_super(&sb);
	memzero_explicit(passphrase, strlen(passphrase));
	free(passphrase);
	return 0;
}

int cmd_set_passphrase(int argc, char *argv[])
{
	struct bch_opts opts = bch2_opts_empty();
	struct bch_fs *c;

	if (argc < 2)
		die("Please supply one or more devices");

	opt_set(opts, nostart, true);

	/*
	 * we use bch2_fs_open() here, instead of just reading the superblock,
	 * to make sure we're opening and updating every component device:
	 */

	c = bch2_fs_open(argv + 1, argc - 1, opts);
	if (IS_ERR(c))
		die("Error opening %s: %s", argv[1], strerror(-PTR_ERR(c)));

	struct bch_sb_field_crypt *crypt = bch2_sb_get_crypt(c->disk_sb);
	if (!crypt)
		die("Filesystem does not have encryption enabled");

	struct bch_encrypted_key new_key;
	new_key.magic = BCH_KEY_MAGIC;

	int ret = bch2_decrypt_sb_key(c, crypt, &new_key.key);
	if (ret)
		die("Error getting current key");

	char *new_passphrase = read_passphrase_twice("Enter new passphrase: ");
	struct bch_key passphrase_key = derive_passphrase(crypt, new_passphrase);

	if (bch2_chacha_encrypt_key(&passphrase_key, __bch2_sb_key_nonce(c->disk_sb),
				    &new_key, sizeof(new_key)))
		die("error encrypting key");
	crypt->key = new_key;

	bch2_write_super(c);
	bch2_fs_stop(c);
	return 0;
}

int cmd_remove_passphrase(int argc, char *argv[])
{
	struct bch_opts opts = bch2_opts_empty();
	struct bch_fs *c;

	if (argc < 2)
		die("Please supply one or more devices");

	opt_set(opts, nostart, true);
	c = bch2_fs_open(argv + 1, argc - 1, opts);
	if (IS_ERR(c))
		die("Error opening %s: %s", argv[1], strerror(-PTR_ERR(c)));

	struct bch_sb_field_crypt *crypt = bch2_sb_get_crypt(c->disk_sb);
	if (!crypt)
		die("Filesystem does not have encryption enabled");

	struct bch_encrypted_key new_key;
	new_key.magic = BCH_KEY_MAGIC;

	int ret = bch2_decrypt_sb_key(c, crypt, &new_key.key);
	if (ret)
		die("Error getting current key");

	crypt->key = new_key;

	bch2_write_super(c);
	bch2_fs_stop(c);
	return 0;
}
