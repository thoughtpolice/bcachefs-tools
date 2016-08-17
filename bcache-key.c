#include <errno.h>
#include <unistd.h>
#include <keyutils.h>
#include <uuid/uuid.h>
#include <nih/command.h>
#include <nih/option.h>

#include "bcache.h"
#include "libbcache.h"
#include "crypto.h"

NihOption opts_unlock[] = {
	NIH_OPTION_LAST
};

int cmd_unlock(NihCommand *command, char * const *args)
{
	struct bcache_disk_key disk_key;
	struct bcache_key key;
	struct cache_sb sb;
	char *passphrase;
	char uuid[40];
	char description[60];

	if (!args[0] || args[1])
		die("please supply a single device");

	bcache_super_read(args[0], &sb);

	if (!CACHE_SET_ENCRYPTION_KEY(&sb))
		die("filesystem is not encrypted");

	memcpy(&disk_key, sb.encryption_key, sizeof(disk_key));

	if (!memcmp(&disk_key, bch_key_header, sizeof(bch_key_header)))
		die("filesystem does not have encryption key");

	passphrase = read_passphrase("Enter passphrase: ");

	derive_passphrase(&key, passphrase);
	disk_key_encrypt(&disk_key, &key);

	if (memcmp(&disk_key, bch_key_header, sizeof(bch_key_header)))
		die("incorrect passphrase");

	uuid_unparse_lower(sb.user_uuid.b, uuid);
	sprintf(description, "bcache:%s", uuid);

	if (add_key("logon", description, &key, sizeof(key),
		    KEY_SPEC_USER_KEYRING) < 0)
		die("add_key error: %s", strerror(errno));

	memzero_explicit(&disk_key, sizeof(disk_key));
	memzero_explicit(&key, sizeof(key));
	memzero_explicit(passphrase, strlen(passphrase));
	free(passphrase);
	return 0;
}
