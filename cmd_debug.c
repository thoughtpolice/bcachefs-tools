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

static void dump_one_device(struct cache_set *c, struct cache *ca, int fd)
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

	/* Prios/gens: */
	for (i = 0; i < prio_buckets(ca); i++)
		range_add(&data,
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
				if (ptr->dev == ca->dev_idx)
					range_add(&data,
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
	struct bch_opts opts = bch_opts_empty();
	struct cache_set *c = NULL;
	const char *err;
	char *out = NULL;
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

	err = bch_fs_open(argv + optind, argc - optind, opts, &c);
	if (err)
		die("error opening %s: %s", argv[optind], err);

	down_read(&c->gc_lock);

	for (i = 0; i < c->sb.nr_devices; i++)
		if (c->cache[i])
			nr_devices++;

	BUG_ON(!nr_devices);

	for (i = 0; i < c->sb.nr_devices; i++) {
		int mode = O_WRONLY|O_CREAT|O_TRUNC;

		if (!force)
			mode |= O_EXCL;

		if (!c->cache[i])
			continue;

		char *path = nr_devices > 1
			? mprintf("%s.%u", out, i)
			: strdup(out);
		fd = xopen(path, mode, 0600);
		free(path);

		dump_one_device(c, c->cache[i], fd);
		close(fd);
	}

	up_read(&c->gc_lock);

	bch_fs_stop(c);
	return 0;
}

static void list_keys(struct cache_set *c, enum btree_id btree_id,
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

static void list_btree_formats(struct cache_set *c, enum btree_id btree_id,
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

static struct bpos parse_pos(char *buf)
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
	     "  -b (extents|inodes|dirents|xattrs)    Btree to list from\n"
	     "  -s inode:offset                       Start position to list from\n"
	     "  -e inode:offset                       End position\n"
	     "  -m (keys|formats)                     List mode\n"
	     "  -h                                    Display this help and exit\n"
	     "Report bugs to <linux-bcache@vger.kernel.org>");
}

static const char * const list_modes[] = {
	"keys",
	"formats",
	NULL
};

int cmd_list(int argc, char *argv[])
{
	struct bch_opts opts = bch_opts_empty();
	struct cache_set *c = NULL;
	enum btree_id btree_id = BTREE_ID_EXTENTS;
	struct bpos start = POS_MIN, end = POS_MAX;
	const char *err;
	int mode = 0, opt;

	opts.nochanges	= true;
	opts.norecovery	= true;
	opts.errors	= BCH_ON_ERROR_CONTINUE;
	fsck_err_opt	= FSCK_ERR_NO;

	while ((opt = getopt(argc, argv, "b:s:e:m:h")) != -1)
		switch (opt) {
		case 'b':
			btree_id = read_string_list_or_die(optarg,
						bch_btree_ids, "btree id");
			break;
		case 's':
			start	= parse_pos(optarg);
			break;
		case 'e':
			end	= parse_pos(optarg);
			break;
		case 'm':
			mode = read_string_list_or_die(optarg,
						list_modes, "list mode");
			break;
		case 'h':
			list_keys_usage();
			exit(EXIT_SUCCESS);
		}

	if (optind >= argc)
		die("Please supply device(s) to check");

	err = bch_fs_open(argv + optind, argc - optind, opts, &c);
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

	bch_fs_stop(c);
	return 0;
}
