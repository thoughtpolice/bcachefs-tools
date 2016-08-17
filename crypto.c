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
#include <sodium/crypto_stream_chacha20.h>

#include "crypto.h"

char *read_passphrase(const char *prompt)
{
	struct termios old, new;
	char *buf = NULL;
	size_t buflen = 0;
	ssize_t ret;

	fprintf(stderr, "%s", prompt);
	fflush(stderr);

	if (tcgetattr(fileno(stdin), &old))
		die("error getting terminal attrs");

	new = old;
	new.c_lflag &= ~ECHO;
	if (tcsetattr(fileno(stdin), TCSAFLUSH, &new))
		die("error setting terminal attrs");

	ret = getline(&buf, &buflen, stdin);
	if (ret <= 0)
		die("error reading passphrase");

	tcsetattr(fileno(stdin), TCSAFLUSH, &old);
	fprintf(stderr, "\n");
	return buf;
}

void derive_passphrase(struct bcache_key *key, const char *passphrase)
{
	const unsigned char salt[] = "bcache";
	int ret;

	ret = libscrypt_scrypt((void *) passphrase, strlen(passphrase),
			       salt, sizeof(salt),
			       SCRYPT_N, SCRYPT_r, SCRYPT_p,
			       (void *) key, sizeof(*key));
	if (ret)
		die("scrypt error: %i", ret);
}

void disk_key_encrypt(struct bcache_disk_key *disk_key,
		      struct bcache_key *key)
{
	int ret;

	ret = crypto_stream_chacha20_xor((void *) disk_key,
					 (void *) disk_key, sizeof(*disk_key),
					 (void *) &bch_master_key_nonce,
					 (void *) key);
	if (ret)
		die("chacha20 error: %i", ret);
}

void disk_key_init(struct bcache_disk_key *disk_key)
{
	ssize_t ret;

	memcpy(&disk_key->header, bch_key_header, sizeof(bch_key_header));
#if 0
	ret = getrandom(disk_key->key, sizeof(disk_key->key), GRND_RANDOM);
	if (ret != sizeof(disk_key->key))
		die("error getting random bytes for key");
#else
	int fd = open("/dev/random", O_RDONLY|O_NONBLOCK);
	if (fd < 0)
		die("error opening /dev/random");

	size_t n = 0;
	struct timespec start;
	bool printed = false;

	clock_gettime(CLOCK_MONOTONIC, &start);

	while (n < sizeof(disk_key->key)) {
		struct timeval timeout = { 1, 0 };
		fd_set set;

		FD_ZERO(&set);
		FD_SET(fd, &set);

		if (select(fd + 1, &set, NULL, NULL, &timeout) < 0)
			die("select error");

		ret = read(fd,
			   (void *) disk_key->key + n,
			   sizeof(disk_key->key) - n);
		if (ret == -1 && errno != EINTR && errno != EAGAIN)
			die("error reading from /dev/random");
		if (ret > 0)
			n += ret;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		now.tv_sec	-= start.tv_sec;
		now.tv_nsec	-= start.tv_nsec;

		while (now.tv_nsec < 0) {
			long nsec_per_sec = 1000 * 1000 * 1000;
			long sec = now.tv_nsec / nsec_per_sec - 1;
			now.tv_nsec	-= sec * nsec_per_sec;
			now.tv_sec	+= sec;
		}

		if (!printed && now.tv_sec >= 3) {
			printf("Reading from /dev/random is taking a long time...\n)");
			printed = true;
		}
	}
	close(fd);
#endif
}
