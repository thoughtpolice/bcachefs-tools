#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <blkid.h>
#include <uuid/uuid.h>

#include "ccan/crc/crc.h"

#include "bcachefs_ioctl.h"
#include "linux/sort.h"
#include "tools-util.h"
#include "util.h"

void die(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);

	exit(EXIT_FAILURE);
}

char *mprintf(const char *fmt, ...)
{
	va_list args;
	char *str;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(&str, fmt, args);
	va_end(args);

	if (ret < 0)
		die("insufficient memory");

	return str;
}

void *xcalloc(size_t count, size_t size)
{
	void *p = calloc(count, size);

	if (!p)
		die("insufficient memory");

	return p;
}

void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (!p)
		die("insufficient memory");

	memset(p, 0, size);
	return p;
}

void xpread(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t r = pread(fd, buf, count, offset);

	if (r != count)
		die("read error (ret %zi)", r);
}

void xpwrite(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t r = pwrite(fd, buf, count, offset);

	if (r != count)
		die("write error (ret %zi err %m)", r);
}

struct stat xfstatat(int dirfd, const char *path, int flags)
{
	struct stat stat;
	if (fstatat(dirfd, path, &stat, flags))
		die("stat error: %m");
	return stat;
}

struct stat xfstat(int fd)
{
	struct stat stat;
	if (fstat(fd, &stat))
		die("stat error: %m");
	return stat;
}

/* Integer stuff: */

struct units_buf __pr_units(u64 v, enum units units)
{
	struct units_buf ret;

	switch (units) {
	case BYTES:
		snprintf(ret.b, sizeof(ret.b), "%llu", v << 9);
		break;
	case SECTORS:
		snprintf(ret.b, sizeof(ret.b), "%llu", v);
		break;
	case HUMAN_READABLE:
		v <<= 9;

		if (v >= 1024) {
			int exp = log(v) / log(1024);
			snprintf(ret.b, sizeof(ret.b), "%.1f%c",
				 v / pow(1024, exp),
				 "KMGTPE"[exp-1]);
		} else {
			snprintf(ret.b, sizeof(ret.b), "%llu", v);
		}

		break;
	}

	return ret;
}

/* Argument parsing stuff: */

/* File parsing (i.e. sysfs) */

char *read_file_str(int dirfd, const char *path)
{
	int fd = xopenat(dirfd, path, O_RDONLY);
	ssize_t len = xfstat(fd).st_size;

	char *buf = xmalloc(len + 1);

	len = read(fd, buf, len);
	if (len < 0)
		die("read error: %m");

	buf[len] = '\0';
	if (len && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	close(fd);

	return buf;
}

u64 read_file_u64(int dirfd, const char *path)
{
	char *buf = read_file_str(dirfd, path);
	u64 v;
	if (kstrtou64(buf, 10, &v))
		die("read_file_u64: error parsing %s (got %s)", path, buf);
	free(buf);
	return v;
}

/* String list options: */

ssize_t read_string_list_or_die(const char *opt, const char * const list[],
				const char *msg)
{
	ssize_t v = bch2_read_string_list(opt, list);
	if (v < 0)
		die("Bad %s %s", msg, opt);

	return v;
}

/* Returns size of file or block device: */
u64 get_size(const char *path, int fd)
{
	struct stat statbuf = xfstat(fd);

	if (!S_ISBLK(statbuf.st_mode))
		return statbuf.st_size;

	u64 ret;
	xioctl(fd, BLKGETSIZE64, &ret);
	return ret;
}

/* Returns blocksize in units of 512 byte sectors: */
unsigned get_blocksize(const char *path, int fd)
{
	struct stat statbuf = xfstat(fd);

	if (!S_ISBLK(statbuf.st_mode))
		return statbuf.st_blksize >> 9;

	unsigned ret;
	xioctl(fd, BLKPBSZGET, &ret);
	return ret >> 9;
}

/* Open a block device, do magic blkid stuff to probe for existing filesystems: */
int open_for_format(const char *dev, bool force)
{
	blkid_probe pr;
	const char *fs_type = NULL, *fs_label = NULL;
	size_t fs_type_len, fs_label_len;

	int fd = xopen(dev, O_RDWR|O_EXCL);

	if (force)
		return fd;

	if (!(pr = blkid_new_probe()))
		die("blkid error 1");
	if (blkid_probe_set_device(pr, fd, 0, 0))
		die("blkid error 2");
	if (blkid_probe_enable_partitions(pr, true))
		die("blkid error 3");
	if (blkid_do_fullprobe(pr) < 0)
		die("blkid error 4");

	blkid_probe_lookup_value(pr, "TYPE", &fs_type, &fs_type_len);
	blkid_probe_lookup_value(pr, "LABEL", &fs_label, &fs_label_len);

	if (fs_type) {
		if (fs_label)
			printf("%s contains a %s filesystem labelled '%s'\n",
			       dev, fs_type, fs_label);
		else
			printf("%s contains a %s filesystem\n",
			       dev, fs_type);
		fputs("Proceed anyway?", stdout);
		if (!ask_yn())
			exit(EXIT_FAILURE);
	}

	blkid_free_probe(pr);
	return fd;
}

/* Global control device: */
int bcachectl_open(void)
{
	return xopen("/dev/bcachefs-ctl", O_RDWR);
}

/* Filesystem handles (ioctl, sysfs dir): */

#define SYSFS_BASE "/sys/fs/bcachefs/"

struct bcache_handle bcache_fs_open(const char *path)
{
	struct bcache_handle ret;
	uuid_t tmp;

	if (!uuid_parse(path, tmp)) {
		/* It's a UUID, look it up in sysfs: */
		char *sysfs = mprintf("%s%s", SYSFS_BASE, path);
		ret.sysfs_fd = xopen(sysfs, O_RDONLY);

		char *minor = read_file_str(ret.sysfs_fd, "minor");
		char *ctl = mprintf("/dev/bcachefs%s-ctl", minor);
		ret.ioctl_fd = xopen(ctl, O_RDWR);

		free(sysfs);
		free(minor);
		free(ctl);
	} else {
		/* It's a path: */
		ret.ioctl_fd = xopen(path, O_RDONLY);

		struct bch_ioctl_query_uuid uuid;
		xioctl(ret.ioctl_fd, BCH_IOCTL_QUERY_UUID, &uuid);

		char uuid_str[40];
		uuid_unparse(uuid.uuid.b, uuid_str);

		char *sysfs = mprintf("%s%s", SYSFS_BASE, uuid_str);
		ret.sysfs_fd = xopen(sysfs, O_RDONLY);
		free(sysfs);
	}

	return ret;
}

bool ask_yn(void)
{
	const char *short_yes = "yY";
	char *buf = NULL;
	size_t buflen = 0;
	bool ret;

	fputs(" (y,n) ", stdout);
	fflush(stdout);

	if (getline(&buf, &buflen, stdin) < 0)
		die("error reading from standard input");

	ret = strchr(short_yes, buf[0]);
	free(buf);
	return ret;
}

static int range_cmp(const void *_l, const void *_r)
{
	const struct range *l = _l, *r = _r;

	if (l->start < r->start)
		return -1;
	if (l->start > r->start)
		return  1;
	return 0;
}

void ranges_sort_merge(ranges *r)
{
	struct range *t, *i;
	ranges tmp = { NULL };

	sort(&darray_item(*r, 0), darray_size(*r),
	     sizeof(darray_item(*r, 0)), range_cmp, NULL);

	/* Merge contiguous ranges: */
	darray_foreach(i, *r) {
		t = tmp.size ?  &tmp.item[tmp.size - 1] : NULL;

		if (t && t->end >= i->start)
			t->end = max(t->end, i->end);
		else
			darray_append(tmp, *i);
	}

	darray_free(*r);
	*r = tmp;
}

void ranges_roundup(ranges *r, unsigned block_size)
{
	struct range *i;

	darray_foreach(i, *r) {
		i->start = round_down(i->start, block_size);
		i->end	= round_up(i->end, block_size);
	}
}

void ranges_rounddown(ranges *r, unsigned block_size)
{
	struct range *i;

	darray_foreach(i, *r) {
		i->start = round_up(i->start, block_size);
		i->end	= round_down(i->end, block_size);
		i->end	= max(i->end, i->start);
	}
}

struct fiemap_extent fiemap_iter_next(struct fiemap_iter *iter)
{
	struct fiemap_extent e;

	BUG_ON(iter->idx > iter->f.fm_mapped_extents);

	if (iter->idx == iter->f.fm_mapped_extents) {
		xioctl(iter->fd, FS_IOC_FIEMAP, &iter->f);

		if (!iter->f.fm_mapped_extents)
			return (struct fiemap_extent) { .fe_length = 0 };

		iter->idx = 0;
	}

	e = iter->f.fm_extents[iter->idx++];
	BUG_ON(!e.fe_length);

	iter->f.fm_start = e.fe_logical + e.fe_length;

	return e;
}

const char *strcmp_prefix(const char *a, const char *a_prefix)
{
	while (*a_prefix && *a == *a_prefix) {
		a++;
		a_prefix++;
	}
	return *a_prefix ? NULL : a;
}

unsigned hatoi_validate(const char *s, const char *msg)
{
	u64 v;

	if (bch2_strtoull_h(s, &v))
		die("bad %s %s", msg, s);

	if (v & (v - 1))
		die("%s must be a power of two", msg);

	v /= 512;

	if (v > USHRT_MAX)
		die("%s too large\n", msg);

	if (!v)
		die("%s too small\n", msg);

	return v;
}
