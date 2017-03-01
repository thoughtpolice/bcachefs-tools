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

#include <uuid/uuid.h>

#include "ccan/crc/crc.h"

#include "linux/bcache-ioctl.h"
#include "linux/sort.h"
#include "tools-util.h"
#include "util.h"

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
	size_t len = xfstat(fd).st_size;

	char *buf = malloc(len + 1);

	xpread(fd, buf, len, 0);

	buf[len] = '\0';
	if (len && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	close(fd);

	return buf;
}

u64 read_file_u64(int dirfd, const char *path)
{
	char *buf = read_file_str(dirfd, path);
	u64 ret = strtoll(buf, NULL, 10);

	free(buf);
	return ret;
}

/* String list options: */

ssize_t read_string_list_or_die(const char *opt, const char * const list[],
				const char *msg)
{
	ssize_t v = bch_read_string_list(opt, list);
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

/* Global control device: */
int bcachectl_open(void)
{
	return xopen("/dev/bcache-ctl", O_RDWR);
}

/* Filesystem handles (ioctl, sysfs dir): */

#define SYSFS_BASE "/sys/fs/bcache/"

struct bcache_handle bcache_fs_open(const char *path)
{
	struct bcache_handle ret;
	uuid_t tmp;

	if (!uuid_parse(path, tmp)) {
		/* It's a UUID, look it up in sysfs: */
		char *sysfs = mprintf("%s%s", SYSFS_BASE, path);
		ret.sysfs_fd = xopen(sysfs, O_RDONLY);

		char *minor = read_file_str(ret.sysfs_fd, "minor");
		char *ctl = mprintf("/dev/bcache%s-ctl", minor);
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
