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

#include <linux/random.h>
#include <libscrypt.h>

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

void bch_sb_crypt_init(struct bch_sb *sb,
		       struct bch_sb_field_crypt *crypt,
		       const char *passphrase)
{
	struct bch_key passphrase_key;

	SET_BCH_CRYPT_KDF_TYPE(crypt, BCH_KDF_SCRYPT);
	SET_BCH_KDF_SCRYPT_N(crypt, ilog2(SCRYPT_N));
	SET_BCH_KDF_SCRYPT_R(crypt, ilog2(SCRYPT_r));
	SET_BCH_KDF_SCRYPT_P(crypt, ilog2(SCRYPT_p));

	derive_passphrase(crypt, &passphrase_key, passphrase);

	crypt->key.magic = BCH_KEY_MAGIC;
	get_random_bytes(&crypt->key.key, sizeof(crypt->key.key));

	assert(!bch_key_is_encrypted(&crypt->key));

	if (bch_chacha_encrypt_key(&passphrase_key, __bch_sb_key_nonce(sb),
				   &crypt->key, sizeof(crypt->key)))
		die("error encrypting key");

	assert(bch_key_is_encrypted(&crypt->key));

	memzero_explicit(&passphrase_key, sizeof(passphrase_key));
}
