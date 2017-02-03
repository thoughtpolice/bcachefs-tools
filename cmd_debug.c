#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cmds.h"
#include "libbcache.h"
#include "qcow2.h"
#include "tools-util.h"

#include "bcache.h"
#include "alloc.h"
#include "btree_cache.h"
#include "btree_iter.h"
#include "buckets.h"
#include "journal.h"
#include "super.h"

static void dump_usage(void)
{
	puts("bcache dump - dump filesystem metadata\n"
	     "Usage: bcache dump [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -o output     Output qcow2 image(s)\n"
	     "  -h            Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

void dump_one_device(struct cache_set *c, struct cache *ca, int fd)
{
	struct cache_sb *sb = ca->disk_sb.sb;
	sparse_data data;
	unsigned i;

	darray_init(data);

	/* Superblock: */
	data_add(&data, SB_SECTOR << 9, __set_bytes(sb, le16_to_cpu(sb->u64s)));

	/* Journal: */
	for (i = 0; i < bch_nr_journal_buckets(ca->disk_sb.sb); i++)
		if (ca->journal.bucket_seq[i] >= c->journal.last_seq_ondisk) {
			u64 bucket = journal_bucket(ca->disk_sb.sb, i);

			data_add(&data,
				 bucket_bytes(ca) * bucket,
				 bucket_bytes(ca));
		}

	/* Prios/gens: */
	for (i = 0; i < prio_buckets(ca); i++)
		data_add(&data,
			 bucket_bytes(ca) * ca->prio_last_buckets[i],
			 bucket_bytes(ca));

	/* Btree: */
	for (i = 0; i < BTREE_ID_NR; i++) {
		const struct bch_extent_ptr *ptr;
		struct btree_iter iter;
		struct btree *b;

		for_each_btree_node(&iter, c, i, POS_MIN, 0, b) {
			struct bkey_s_c_extent e = bkey_i_to_s_c_extent(&b->key);

			extent_for_each_ptr(e, ptr)
				if (ptr->dev == ca->sb.nr_this_dev)
					data_add(&data,
						 ptr->offset << 9,
						 b->written << 9);
		}
		bch_btree_iter_unlock(&iter);
	}

	qcow2_write_image(ca->disk_sb.bdev->bd_fd, fd, &data,
			  max_t(unsigned, btree_bytes(c) / 8, block_bytes(c)));
}

int cmd_dump(int argc, char *argv[])
{
	DECLARE_COMPLETION_ONSTACK(shutdown);
	struct cache_set_opts opts = cache_set_opts_empty();
	struct cache_set *c = NULL;
	const char *err;
	char *out = NULL, *buf;
	unsigned i, nr_devices = 0;
	bool force = false;
	int fd, opt;

	opts.nochanges	= true;
	opts.noreplay	= true;
	opts.errors	= BCH_ON_ERROR_CONTINUE;
	fsck_err_opt	= FSCK_ERR_NO;

	while ((opt = getopt(argc, argv, "o:fh")) != -1)
		switch (opt) {
		case 'o':
			out = optarg;
			break;
		case 'f':
			force = true;
			break;
		case 'h':
			dump_usage();
			exit(EXIT_SUCCESS);
		}

	if (optind >= argc)
		die("Please supply device(s) to check");

	if (!out)
		die("Please supply output filename");

	buf = alloca(strlen(out) + 10);
	strcpy(buf, out);

	err = bch_register_cache_set(argv + optind, argc - optind, opts, &c);
	if (err)
		die("error opening %s: %s", argv[optind], err);

	down_read(&c->gc_lock);

	for (i = 0; i < c->sb.nr_in_set; i++)
		if (c->cache[i])
			nr_devices++;

	BUG_ON(!nr_devices);

	for (i = 0; i < c->sb.nr_in_set; i++) {
		int mode = O_WRONLY|O_CREAT|O_TRUNC;

		if (!force)
			mode |= O_EXCL;

		if (!c->cache[i])
			continue;

		if (nr_devices > 1)
			sprintf(buf, "%s.%u", out, i);

		fd = open(buf, mode, 0600);
		if (fd < 0)
			die("error opening %s: %s", buf, strerror(errno));

		dump_one_device(c, c->cache[i], fd);
		close(fd);
	}

	up_read(&c->gc_lock);

	c->stop_completion = &shutdown;
	bch_cache_set_stop(c);
	closure_put(&c->cl);
	wait_for_completion(&shutdown);
	return 0;
}

void list_keys(struct cache_set *c, enum btree_id btree_id,
	       struct bpos start, struct bpos end, int mode)
{
	struct btree_iter iter;
	struct bkey_s_c k;
	char buf[512];

	for_each_btree_key(&iter, c, btree_id, start, k) {
		if (bkey_cmp(k.k->p, end) > 0)
			break;

		bch_bkey_val_to_text(c, bkey_type(0, btree_id),
				     buf, sizeof(buf), k);
		puts(buf);
	}
	bch_btree_iter_unlock(&iter);
}

void list_btree_formats(struct cache_set *c, enum btree_id btree_id,
			struct bpos start, struct bpos end, int mode)
{
	struct btree_iter iter;
	struct btree *b;
	char buf[4096];

	for_each_btree_node(&iter, c, btree_id, start, 0, b) {
		if (bkey_cmp(b->key.k.p, end) > 0)
			break;

		bch_print_btree_node(c, b, buf, sizeof(buf));
		puts(buf);
	}
	bch_btree_iter_unlock(&iter);
}

struct bpos parse_pos(char *buf)
{
	char *s = buf;
	char *inode	= strsep(&s, ":");
	char *offset	= strsep(&s, ":");
	struct bpos ret = { 0 };

	if (!inode || !offset || s ||
	    kstrtoull(inode, 10, &ret.inode) ||
	    kstrtoull(offset, 10, &ret.offset))
		die("invalid bpos %s", buf);

	return ret;
}

static void list_keys_usage(void)
{
	puts("bcache list_keys - list filesystem metadata to stdout\n"
	     "Usage: bcache list_keys [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -b btree_id   Integer btree id to list\n"
	     "  -s start      Start pos (as inode:offset)\n"
	     "  -e end        End pos\n"
	     "  -m mode       Mode for listing\n"
	     "  -h            Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

int cmd_list(int argc, char *argv[])
{
	DECLARE_COMPLETION_ONSTACK(shutdown);
	struct cache_set_opts opts = cache_set_opts_empty();
	struct cache_set *c = NULL;
	enum btree_id btree_id = BTREE_ID_EXTENTS;
	struct bpos start = POS_MIN, end = POS_MAX;
	const char *err;
	int mode = 0, opt;
	u64 v;

	opts.nochanges	= true;
	opts.norecovery	= true;
	opts.errors	= BCH_ON_ERROR_CONTINUE;
	fsck_err_opt	= FSCK_ERR_NO;

	while ((opt = getopt(argc, argv, "b:s:e:m:h")) != -1)
		switch (opt) {
		case 'b':
			if (kstrtoull(optarg, 10, &v) ||
			    v >= BTREE_ID_NR)
				die("invalid btree id");
			btree_id = v;
			break;
		case 's':
			start	= parse_pos(optarg);
			break;
		case 'e':
			end	= parse_pos(optarg);
			break;
		case 'm':
			break;
		case 'h':
			list_keys_usage();
			exit(EXIT_SUCCESS);
		}

	if (optind >= argc)
		die("Please supply device(s) to check");

	err = bch_register_cache_set(argv + optind, argc - optind, opts, &c);
	if (err)
		die("error opening %s: %s", argv[optind], err);

	switch (mode) {
	case 0:
		list_keys(c, btree_id, start, end, mode);
		break;
	case 1:
		list_btree_formats(c, btree_id, start, end, mode);
		break;
	default:
		die("Invalid mode");
	}

	c->stop_completion = &shutdown;
	bch_cache_set_stop(c);
	closure_put(&c->cl);
	wait_for_completion(&shutdown);
	return 0;
}
