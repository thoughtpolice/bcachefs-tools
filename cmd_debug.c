#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cmds.h"
#include "libbcachefs.h"
#include "qcow2.h"
#include "tools-util.h"

#include "libbcachefs/bcachefs.h"
#include "libbcachefs/alloc.h"
#include "libbcachefs/btree_cache.h"
#include "libbcachefs/btree_iter.h"
#include "libbcachefs/buckets.h"
#include "libbcachefs/error.h"
#include "libbcachefs/journal.h"
#include "libbcachefs/super.h"

static void dump_usage(void)
{
	puts("bcachefs dump - dump filesystem metadata\n"
	     "Usage: bcachefs dump [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -o output     Output qcow2 image(s)\n"
	     "  -f            Force; overwrite when needed\n"
	     "  -h            Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

static void dump_one_device(struct bch_fs *c, struct bch_dev *ca, int fd)
{
	struct bch_sb *sb = ca->disk_sb.sb;
	ranges data;
	unsigned i;

	darray_init(data);

	/* Superblock: */
	range_add(&data, BCH_SB_LAYOUT_SECTOR << 9,
		  sizeof(struct bch_sb_layout));

	for (i = 0; i < sb->layout.nr_superblocks; i++)
		range_add(&data,
			  le64_to_cpu(sb->layout.sb_offset[i]) << 9,
			  vstruct_bytes(sb));

	/* Journal: */
	for (i = 0; i < ca->journal.nr; i++)
		if (ca->journal.bucket_seq[i] >= c->journal.last_seq_ondisk) {
			u64 bucket = ca->journal.buckets[i];

			range_add(&data,
				  bucket_bytes(ca) * bucket,
				  bucket_bytes(ca));
		}

	/* Btree: */
	for (i = 0; i < BTREE_ID_NR; i++) {
		const struct bch_extent_ptr *ptr;
		struct btree_iter iter;
		struct btree *b;

		for_each_btree_node(&iter, c, i, POS_MIN, 0, b) {
			struct bkey_s_c_extent e = bkey_i_to_s_c_extent(&b->key);

			extent_for_each_ptr(e, ptr)
				if (ptr->dev == ca->dev_idx)
					range_add(&data,
						  ptr->offset << 9,
						  b->written << 9);
		}
		bch2_btree_iter_unlock(&iter);
	}

	qcow2_write_image(ca->disk_sb.bdev->bd_fd, fd, &data,
			  max_t(unsigned, btree_bytes(c) / 8, block_bytes(c)));
}

int cmd_dump(int argc, char *argv[])
{
	struct bch_opts opts = bch2_opts_empty();
	struct bch_dev *ca;
	char *out = NULL;
	unsigned i, nr_devices = 0;
	bool force = false;
	int fd, opt;

	opt_set(opts, nochanges,	true);
	opt_set(opts, noreplay,		true);
	opt_set(opts, degraded,		true);
	opt_set(opts, errors,		BCH_ON_ERROR_CONTINUE);

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
	args_shift(optind);

	if (!out)
		die("Please supply output filename");

	if (!argc)
		die("Please supply device(s) to check");

	struct bch_fs *c = bch2_fs_open(argv, argc, opts);
	if (IS_ERR(c))
		die("error opening %s: %s", argv[0], strerror(-PTR_ERR(c)));

	down_read(&c->gc_lock);

	for_each_online_member(ca, c, i)
		nr_devices++;

	BUG_ON(!nr_devices);

	for_each_online_member(ca, c, i) {
		int flags = O_WRONLY|O_CREAT|O_TRUNC;

		if (!force)
			flags |= O_EXCL;

		if (!c->devs[i])
			continue;

		char *path = nr_devices > 1
			? mprintf("%s.%u", out, i)
			: strdup(out);
		fd = xopen(path, flags, 0600);
		free(path);

		dump_one_device(c, ca, fd);
		close(fd);
	}

	up_read(&c->gc_lock);

	bch2_fs_stop(c);
	return 0;
}

static void list_keys(struct bch_fs *c, enum btree_id btree_id,
		      struct bpos start, struct bpos end)
{
	struct btree_iter iter;
	struct bkey_s_c k;
	char buf[512];

	for_each_btree_key(&iter, c, btree_id, start,
			   BTREE_ITER_PREFETCH, k) {
		if (bkey_cmp(k.k->p, end) > 0)
			break;

		bch2_bkey_val_to_text(c, bkey_type(0, btree_id),
				      buf, sizeof(buf), k);
		puts(buf);
	}
	bch2_btree_iter_unlock(&iter);
}

static void list_btree_formats(struct bch_fs *c, enum btree_id btree_id,
			       struct bpos start, struct bpos end)
{
	struct btree_iter iter;
	struct btree *b;
	char buf[4096];

	for_each_btree_node(&iter, c, btree_id, start, 0, b) {
		if (bkey_cmp(b->key.k.p, end) > 0)
			break;

		bch2_print_btree_node(c, b, buf, sizeof(buf));
		puts(buf);
	}
	bch2_btree_iter_unlock(&iter);
}

static void list_nodes_keys(struct bch_fs *c, enum btree_id btree_id,
			    struct bpos start, struct bpos end)
{
	struct btree_iter iter;
	struct btree_node_iter node_iter;
	struct bkey unpacked;
	struct bkey_s_c k;
	struct btree *b;
	char buf[4096];

	for_each_btree_node(&iter, c, btree_id, start, 0, b) {
		if (bkey_cmp(b->key.k.p, end) > 0)
			break;

		bch2_print_btree_node(c, b, buf, sizeof(buf));
		fputs(buf, stdout);

		buf[0] = '\t';

		for_each_btree_node_key_unpack(b, k, &node_iter,
					       btree_node_is_extents(b),
					       &unpacked) {
			bch2_bkey_val_to_text(c, bkey_type(0, btree_id),
					      buf + 1, sizeof(buf) - 1, k);
			puts(buf);
		}
	}
	bch2_btree_iter_unlock(&iter);
}

static struct bpos parse_pos(char *buf)
{
	char *s = buf, *field;
	u64 inode_v = 0, offset_v = 0;

	if (!(field = strsep(&s, ":")) ||
	    kstrtoull(field, 10, &inode_v))
		die("invalid bpos %s", buf);

	if ((field = strsep(&s, ":")) &&
	    kstrtoull(field, 10, &offset_v))
		die("invalid bpos %s", buf);

	if (s)
		die("invalid bpos %s", buf);

	return (struct bpos) { .inode = inode_v, .offset = offset_v };
}

static void list_keys_usage(void)
{
	puts("bcachefs list - list filesystem metadata to stdout\n"
	     "Usage: bcachefs list [OPTION]... <devices>\n"
	     "\n"
	     "Options:\n"
	     "  -b (extents|inodes|dirents|xattrs)    Btree to list from\n"
	     "  -s inode:offset                       Start position to list from\n"
	     "  -e inode:offset                       End position\n"
	     "  -i inode                              List keys for a given inode number\n"
	     "  -m (keys|formats)                     List mode\n"
	     "  -f                                    Check (fsck) the filesystem first\n"
	     "  -v                                    Verbose mode\n"
	     "  -h                                    Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

static const char * const list_modes[] = {
	"keys",
	"formats",
	"nodes",
	NULL
};

int cmd_list(int argc, char *argv[])
{
	struct bch_opts opts = bch2_opts_empty();
	enum btree_id btree_id = BTREE_ID_EXTENTS;
	struct bpos start = POS_MIN, end = POS_MAX;
	u64 inum;
	int mode = 0, opt;

	opt_set(opts, nochanges,	true);
	opt_set(opts, norecovery,	true);
	opt_set(opts, degraded,		true);
	opt_set(opts, errors,		BCH_ON_ERROR_CONTINUE);

	while ((opt = getopt(argc, argv, "b:s:e:i:m:fvh")) != -1)
		switch (opt) {
		case 'b':
			btree_id = read_string_list_or_die(optarg,
						bch2_btree_ids, "btree id");
			break;
		case 's':
			start	= parse_pos(optarg);
			break;
		case 'e':
			end	= parse_pos(optarg);
			break;
		case 'i':
			if (kstrtoull(optarg, 10, &inum))
				die("invalid inode %s", optarg);
			start	= POS(inum, 0);
			end	= POS(inum + 1, 0);
			break;
		case 'm':
			mode = read_string_list_or_die(optarg,
						list_modes, "list mode");
			break;
		case 'f':
			opt_set(opts, fix_errors, FSCK_OPT_YES);
			opt_set(opts, norecovery, false);
			break;
		case 'v':
			opt_set(opts, verbose_recovery, true);
			break;
		case 'h':
			list_keys_usage();
			exit(EXIT_SUCCESS);
		}
	args_shift(optind);

	if (!argc)
		die("Please supply device(s)");

	struct bch_fs *c = bch2_fs_open(argv, argc, opts);
	if (IS_ERR(c))
		die("error opening %s: %s", argv[0], strerror(-PTR_ERR(c)));

	switch (mode) {
	case 0:
		list_keys(c, btree_id, start, end);
		break;
	case 1:
		list_btree_formats(c, btree_id, start, end);
		break;
	case 2:
		list_nodes_keys(c, btree_id, start, end);
		break;
	default:
		die("Invalid mode");
	}

	bch2_fs_stop(c);
	return 0;
}
