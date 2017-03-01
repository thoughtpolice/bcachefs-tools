#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <keyutils.h>
#include <linux/random.h>
#include <libscrypt.h>
#include <uuid/uuid.h>

#include "checksum.h"
#include "crypto.h"

char *read_passphrase(const char *prompt)
{
	char *buf = NULL;
	size_t buflen = 0;
	ssize_t len;

	if (isatty(STDIN_FILENO)) {
		struct termios old, new;

		fprintf(stderr, "%s", prompt);
		fflush(stderr);

		if (tcgetattr(STDIN_FILENO, &old))
			die("error getting terminal attrs");

		new = old;
		new.c_lflag &= ~ECHO;
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new))
			die("error setting terminal attrs");

		len = getline(&buf, &buflen, stdin);

		tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
		fprintf(stderr, "\n");
	} else {
		len = getline(&buf, &buflen, stdin);
	}

	if (len < 0)
		die("error reading passphrase");
	if (len && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	return buf;
}

void derive_passphrase(struct bch_sb_field_crypt *crypt,
		       struct bch_key *key,
		       const char *passphrase)
{
	const unsigned char salt[] = "bcache";
	int ret;

	switch (BCH_CRYPT_KDF_TYPE(crypt)) {
	case BCH_KDF_SCRYPT:
		ret = libscrypt_scrypt((void *) passphrase, strlen(passphrase),
				       salt, sizeof(salt),
				       1ULL << BCH_KDF_SCRYPT_N(crypt),
				       1ULL << BCH_KDF_SCRYPT_R(crypt),
				       1ULL << BCH_KDF_SCRYPT_P(crypt),
				       (void *) key, sizeof(*key));
		if (ret)
			die("scrypt error: %i", ret);
		break;
	default:
		die("unknown kdf type %llu", BCH_CRYPT_KDF_TYPE(crypt));
	}
}

void add_bcache_key(struct bch_sb *sb, const char *passphrase)
{
	struct bch_sb_field_crypt *crypt = bch_sb_get_crypt(sb);
	if (!crypt)
		die("filesystem is not encrypted");

	struct bch_encrypted_key sb_key = crypt->key;
	if (!bch_key_is_encrypted(&sb_key))
		die("filesystem does not have encryption key");

	struct bch_key passphrase_key;
	derive_passphrase(crypt, &passphrase_key, passphrase);

	/* Check if the user supplied the correct passphrase: */
	if (bch_chacha_encrypt_key(&passphrase_key, __bch_sb_key_nonce(sb),
				   &sb_key, sizeof(sb_key)))
		die("error encrypting key");

	if (bch_key_is_encrypted(&sb_key))
		die("incorrect passphrase");

	char uuid[40];
	uuid_unparse_lower(sb->user_uuid.b, uuid);

	char *description = mprintf("bcache:%s", uuid);

	if (add_key("logon", description,
		    &passphrase_key, sizeof(passphrase_key),
		    KEY_SPEC_USER_KEYRING) < 0 ||
	    add_key("user", description,
		    &passphrase_key, sizeof(passphrase_key),
		    KEY_SPEC_USER_KEYRING) < 0)
		die("add_key error: %s", strerror(errno));

	memzero_explicit(description, strlen(description));
	free(description);
	memzero_explicit(&passphrase_key, sizeof(passphrase_key));
	memzero_explicit(&sb_key, sizeof(sb_key));
}

void bch_sb_crypt_init(struct bch_sb *sb,
		       struct bch_sb_field_crypt *crypt,
		       const char *passphrase)
{
	crypt->key.magic = BCH_KEY_MAGIC;
	get_random_bytes(&crypt->key.key, sizeof(crypt->key.key));

	if (passphrase) {
		struct bch_key passphrase_key;

		SET_BCH_CRYPT_KDF_TYPE(crypt, BCH_KDF_SCRYPT);
		SET_BCH_KDF_SCRYPT_N(crypt, ilog2(SCRYPT_N));
		SET_BCH_KDF_SCRYPT_R(crypt, ilog2(SCRYPT_r));
		SET_BCH_KDF_SCRYPT_P(crypt, ilog2(SCRYPT_p));

		derive_passphrase(crypt, &passphrase_key, passphrase);

		assert(!bch_key_is_encrypted(&crypt->key));

		if (bch_chacha_encrypt_key(&passphrase_key, __bch_sb_key_nonce(sb),
					   &crypt->key, sizeof(crypt->key)))
			die("error encrypting key");

		assert(bch_key_is_encrypted(&crypt->key));

		memzero_explicit(&passphrase_key, sizeof(passphrase_key));
	}
}
