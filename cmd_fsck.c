
#include "cmds.h"
#include "libbcachefs/error.h"
#include "libbcachefs.h"
#include "libbcachefs/super.h"
#include "tools-util.h"

static void usage(void)
{
	puts("bcachefs fsck - filesystem check and repair\n"
	     "Usage: bcachefs fsck [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -p     Automatic repair (no questions)\n"
	     "  -n     Don't repair, only check for errors\n"
	     "  -y     Assume \"yes\" to all questions\n"
	     "  -f     Force checking even if filesystem is marked clean\n"
	     "  -v     Be verbose\n"
	     " --h     Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

int cmd_fsck(int argc, char *argv[])
{
	struct bch_opts opts = bch2_opts_empty();
	int opt;

	opt_set(opts, degraded, true);
	opt_set(opts, fix_errors, FSCK_OPT_ASK);

	while ((opt = getopt(argc, argv, "apynfvh")) != -1)
		switch (opt) {
		case 'a': /* outdated alias for -p */
		case 'p':
			opt_set(opts, fix_errors, FSCK_OPT_YES);
			break;
		case 'y':
			opt_set(opts, fix_errors, FSCK_OPT_YES);
			break;
		case 'n':
			opt_set(opts, nochanges, true);
			opt_set(opts, fix_errors, FSCK_OPT_NO);
			break;
		case 'f':
			/* force check, even if filesystem marked clean: */
			break;
		case 'v':
			opt_set(opts, verbose_recovery, true);
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		}
	args_shift(optind);

	if (!argc)
		die("Please supply device(s) to check");

	struct bch_fs *c = bch2_fs_open(argv, argc, opts);
	if (IS_ERR(c))
		die("error opening %s: %s", argv[0], strerror(-PTR_ERR(c)));

	bch2_fs_stop(c);
	return 0;
}
