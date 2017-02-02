#include <alloca.h>
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
	int fd = openat(dirfd, path, O_RDONLY);

	if (fd < 0)
		die("Unable to open %s\n", path);

	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0)
		die("fstat error\n");

	char *buf = malloc(statbuf.st_size + 1);

	int len = read(fd, buf, statbuf.st_size);
	if (len < 0)
		die("read error while reading from file %s\n", path);

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

/* Returns size of file or block device, in units of 512 byte sectors: */
u64 get_size(const char *path, int fd)
{
	struct stat statbuf;
	if (fstat(fd, &statbuf))
		die("Error statting %s: %s", path, strerror(errno));

	if (!S_ISBLK(statbuf.st_mode))
		return statbuf.st_size >> 9;

	u64 ret;
	if (ioctl(fd, BLKGETSIZE64, &ret))
		die("Error getting block device size on %s: %s\n",
		    path, strerror(errno));

	return ret >> 9;
}

/* Returns blocksize in units of 512 byte sectors: */
unsigned get_blocksize(const char *path, int fd)
{
	struct stat statbuf;
	if (fstat(fd, &statbuf))
		die("Error statting %s: %s", path, strerror(errno));

	if (!S_ISBLK(statbuf.st_mode))
		return statbuf.st_blksize >> 9;

	unsigned ret;
	if (ioctl(fd, BLKPBSZGET, &ret))
		die("Error getting blocksize on %s: %s\n",
		    path, strerror(errno));

	return ret >> 9;
}

/* Global control device: */
int bcachectl_open(void)
{
	int fd = open("/dev/bcache-ctl", O_RDWR);
	if (fd < 0)
		die("Can't open bcache device: %s", strerror(errno));

	return fd;
}

/* Filesystem handles (ioctl, sysfs dir): */

#define SYSFS_BASE "/sys/fs/bcache/"

struct bcache_handle bcache_fs_open(const char *path)
{
	struct bcache_handle ret;
	uuid_t tmp;

	if (!uuid_parse(path, tmp)) {
		/* It's a UUID, look it up in sysfs: */

		char *sysfs = alloca(strlen(SYSFS_BASE) + strlen(path) + 1);
		sprintf(sysfs, "%s%s", SYSFS_BASE, path);

		ret.sysfs_fd = open(sysfs, O_RDONLY);
		if (!ret.sysfs_fd)
			die("Unable to open %s\n", path);

		char *minor = read_file_str(ret.sysfs_fd, "minor");
		char *ctl = alloca(20 + strlen(minor));

		sprintf(ctl, "/dev/bcache%s-ctl", minor);
		free(minor);

		ret.ioctl_fd = open(ctl, O_RDWR);
		if (ret.ioctl_fd < 0)
			die("Error opening control device: %s\n",
			    strerror(errno));
	} else {
		/* It's a path: */

		ret.ioctl_fd = open(path, O_RDONLY);
		if (ret.ioctl_fd < 0)
			die("Error opening %s: %s\n",
			    path, strerror(errno));

		struct bch_ioctl_query_uuid uuid;
		if (ioctl(ret.ioctl_fd, BCH_IOCTL_QUERY_UUID, &uuid))
			die("ioctl error (not a bcache fs?): %s\n",
			    strerror(errno));

		char uuid_str[40];
		uuid_unparse(uuid.uuid.b, uuid_str);

		char *sysfs = alloca(strlen(SYSFS_BASE) + strlen(uuid_str) + 1);
		sprintf(sysfs, "%s%s", SYSFS_BASE, uuid_str);

		ret.sysfs_fd = open(sysfs, O_RDONLY);
		if (ret.sysfs_fd < 0)
			die("Unable to open sysfs dir %s: %s\n",
			    sysfs, strerror(errno));
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
