#include <errno.h>
#include <unistd.h>
#include <keyutils.h>
#include <uuid/uuid.h>

#include "cmds.h"
#include "checksum.h"
#include "crypto.h"
#include "libbcache.h"

int cmd_unlock(int argc, char *argv[])
{
	struct bch_encrypted_key sb_key;
	struct bch_key passphrase_key;
	struct bch_sb *sb;
	struct bch_sb_field_crypt *crypt;
	char *passphrase;
	char uuid[40];
	char description[60];

	if (argc != 2)
		die("please supply a single device");

	sb = bcache_super_read(argv[1]);

	crypt = bch_sb_get_crypt(sb);
	if (!crypt)
		die("filesystem is not encrypted");

	sb_key = crypt->key;

	if (!bch_key_is_encrypted(&sb_key))
		die("filesystem does not have encryption key");

	passphrase = read_passphrase("Enter passphrase: ");
	derive_passphrase(crypt, &passphrase_key, passphrase);

	/* Check if the user supplied the correct passphrase: */
	if (bch_chacha_encrypt_key(&passphrase_key, __bch_sb_key_nonce(sb),
				   &sb_key, sizeof(sb_key)))
		die("error encrypting key");

	if (bch_key_is_encrypted(&sb_key))
		die("incorrect passphrase");

	uuid_unparse_lower(sb->user_uuid.b, uuid);
	sprintf(description, "bcache:%s", uuid);

	if (add_key("logon", description,
		    &passphrase_key, sizeof(passphrase_key),
		    KEY_SPEC_USER_KEYRING) < 0 ||
	    add_key("user", description,
		    &passphrase_key, sizeof(passphrase_key),
		    KEY_SPEC_USER_KEYRING) < 0)
		die("add_key error: %s", strerror(errno));

	memzero_explicit(&sb_key, sizeof(sb_key));
	memzero_explicit(&passphrase_key, sizeof(passphrase_key));
	memzero_explicit(passphrase, strlen(passphrase));
	free(passphrase);
	return 0;
}
