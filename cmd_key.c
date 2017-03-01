#include <errno.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "cmds.h"
#include "checksum.h"
#include "crypto.h"
#include "libbcache.h"

int cmd_unlock(int argc, char *argv[])
{
	struct bch_sb *sb;
	char *passphrase;

	if (argc != 2)
		die("please supply a single device");

	sb = bcache_super_read(argv[1]);

	passphrase = read_passphrase("Enter passphrase: ");

	add_bcache_key(sb, passphrase);

	memzero_explicit(passphrase, strlen(passphrase));
	free(passphrase);
	return 0;
}
