
#include <nih/command.h>
#include <nih/option.h>

#include "bcache.h"
#include "bcache-fs.h"

struct bcache_fs {
	/* options... */

	u64		capacity;

	/* XXX: dirty != used, it doesn't count metadata */
	u64		bytes_dirty;
};

static struct bcache_fs fill_fs(struct bcache_handle fs)
{
	return (struct bcache_fs) {
	};
}

NihOption opts_fs_show[] = {
//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}
	NIH_OPTION_LAST
};

int cmd_fs_show(NihCommand *command, char *const *args)
{
	if (nr_args(args) != 1)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(args[0]);

	return 0;
}

NihOption opts_fs_set[] = {
//	{ int shortoption, char *longoption, char *help, NihOptionGroup, char *argname, void *value, NihOptionSetter}
	NIH_OPTION_LAST
};

int cmd_fs_set(NihCommand *command, char *const *args)
{
	if (nr_args(args) < 1)
		die("Please supply a filesystem");

	struct bcache_handle fs = bcache_fs_open(args[0]);

	return 0;
}
