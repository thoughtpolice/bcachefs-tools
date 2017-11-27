#include <errno.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "cmds.h"
#include "libbcachefs/checksum.h"
#include "crypto.h"
#include "libbcachefs.h"

int cmd_unlock(int argc, char *argv[])
{
	struct bch_sb_handle sb;
	const char *err;
	char *passphrase;

	if (argc != 2)
		die("Please supply a single device");

	err = bch2_read_super(argv[1], bch2_opts_empty(), &sb);
	if (err)
		die("Error opening %s: %s", argv[1], err);

	passphrase = read_passphrase("Enter passphrase: ");

	bch2_add_key(sb.sb, passphrase);

	memzero_explicit(passphrase, strlen(passphrase));
	free(passphrase);
	return 0;
}

int cmd_set_passphrase(int argc, char *argv[])
{
	struct bch_opts opts = bch2_opts_empty();
	struct bch_fs *c = NULL;
	const char *err;

	if (argc < 2)
		die("Please supply one or more devices");

	opt_set(opts, nostart, true);
	err = bch2_fs_open(argv + 1, argc - 1, opts, &c);
	if (err)
		die("Error opening %s: %s", argv[1], err);

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
	struct bch_fs *c = NULL;
	const char *err;

	if (argc < 2)
		die("Please supply one or more devices");

	opt_set(opts, nostart, true);
	err = bch2_fs_open(argv + 1, argc - 1, opts, &c);
	if (err)
		die("Error opening %s: %s", argv[1], err);

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
