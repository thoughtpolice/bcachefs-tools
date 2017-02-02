
#include "cmds.h"
#include "libbcache.h"
#include "super.h"
#include "tools-util.h"

static void usage(void)
{
	puts("bcache fsck - filesystem check and repair\n"
	     "Usage: bcache fsck [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -p     Automatic repair (no questions\n"
	     "  -n     Don't repair, only check for errors\n"
	     "  -y     Assume \"yes\" to all questions\n"
	     "  -f     Force checking even if filesystem is marked clean\n"
	     "  -v     Be verbose\n"
	     " --h     Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

int cmd_fsck(int argc, char *argv[])
{
	DECLARE_COMPLETION_ONSTACK(shutdown);
	struct cache_set_opts opts = cache_set_opts_empty();
	struct cache_set *c = NULL;
	const char *err;
	int opt;

	while ((opt = getopt(argc, argv, "pynfvh")) != -1)
		switch (opt) {
		case 'p':
			fsck_err_opt = FSCK_ERR_YES;
			break;
		case 'y':
			fsck_err_opt = FSCK_ERR_YES;
			break;
		case 'n':
			opts.nochanges = true;
			fsck_err_opt = FSCK_ERR_NO;
			break;
		case 'f':
			/* force check, even if filesystem marked clean: */
			break;
		case 'v':
			opts.verbose_recovery = true;
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		}

	if (optind >= argc)
		die("Please supply device(s) to check");

	err = bch_register_cache_set(argv + optind, argc - optind, opts, &c);
	if (err)
		die("error opening %s: %s", argv[optind], err);

	c->stop_completion = &shutdown;
	bch_cache_set_stop(c);
	closure_put(&c->cl);

	/* Killable? */
	wait_for_completion(&shutdown);

	return 0;
}
