#include <errno.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "cmds.h"
#include "checksum.h"
#include "crypto.h"
#include "libbcachefs.h"

int cmd_unlock(int argc, char *argv[])
{
	struct bch_sb *sb;
	char *passphrase;

	if (argc != 2)
		die("please supply a single device");

	sb = bch2_super_read(argv[1]);

	passphrase = read_passphrase("Enter passphrase: ");

	bch2_add_key(sb, passphrase);

	memzero_explicit(passphrase, strlen(passphrase));
	free(passphrase);
	return 0;
}
