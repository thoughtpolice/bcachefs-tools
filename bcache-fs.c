
#include "bcache-cmds.h"

struct bcache_fs {
	/* options... */

	u64		capacity;

	/* XXX: dirty != used, it doesn't count metadata */
	u64		bytes_dirty;
};

#if 0
static struct bcache_fs fill_fs(struct bcache_handle fs)
{
	return (struct bcache_fs) {
	};
}
#endif

int cmd_fs_show(int argc, char *argv[])
{
	if (argc != 2)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(argv[1]);

	fs = fs;
	return 0;
}

int cmd_fs_set(int argc, char *argv[])
{
	if (argc != 2)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(argv[1]);

	fs = fs;
	return 0;
}
