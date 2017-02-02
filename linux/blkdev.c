
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/fs.h>

int submit_bio_wait(struct bio *bio)
{
	struct iovec *iov;
	struct bvec_iter iter;
	struct bio_vec bv;
	ssize_t ret;
	unsigned i;

	if (bio->bi_opf & REQ_PREFLUSH)
		fdatasync(bio->bi_bdev->bd_fd);

	i = 0;
	bio_for_each_segment(bv, bio, iter)
		i++;

	iov = alloca(sizeof(*iov) * i);

	i = 0;
	bio_for_each_segment(bv, bio, iter)
		iov[i++] = (struct iovec) {
			.iov_base = page_address(bv.bv_page) + bv.bv_offset,
			.iov_len = bv.bv_len,
		};

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		ret = preadv(bio->bi_bdev->bd_fd, iov, i,
			     bio->bi_iter.bi_sector << 9);
		break;
	case REQ_OP_WRITE:
		ret = pwritev(bio->bi_bdev->bd_fd, iov, i,
			      bio->bi_iter.bi_sector << 9);
		break;
	default:
		BUG();
	}

	if (bio->bi_opf & REQ_FUA)
		fdatasync(bio->bi_bdev->bd_fd);

	return ret == bio->bi_iter.bi_size ? 0 : -EIO;
}

void generic_make_request(struct bio *bio)
{
	bio->bi_error = submit_bio_wait(bio);
	bio_endio(bio);
}

int blkdev_issue_discard(struct block_device *bdev,
			 sector_t sector, sector_t nr_sects,
			 gfp_t gfp_mask, unsigned long flags)
{
	return 0;
}

unsigned bdev_logical_block_size(struct block_device *bdev)
{
	struct stat statbuf;
	unsigned blksize;
	int ret;

	ret = fstat(bdev->bd_fd, &statbuf);
	BUG_ON(ret);

	if (!S_ISBLK(statbuf.st_mode))
		return statbuf.st_blksize >> 9;

	ret = ioctl(bdev->bd_fd, BLKPBSZGET, &blksize);
	BUG_ON(ret);

	return blksize >> 9;
}

sector_t get_capacity(struct gendisk *disk)
{
	struct block_device *bdev =
		container_of(disk, struct block_device, __bd_disk);
	struct stat statbuf;
	u64 bytes;
	int ret;

	ret = fstat(bdev->bd_fd, &statbuf);
	BUG_ON(ret);

	if (!S_ISBLK(statbuf.st_mode))
		return statbuf.st_size >> 9;

	ret = ioctl(bdev->bd_fd, BLKGETSIZE64, &bytes);
	BUG_ON(ret);

	return bytes >> 9;
}

void blkdev_put(struct block_device *bdev, fmode_t mode)
{
	fdatasync(bdev->bd_fd);
	close(bdev->bd_fd);
	free(bdev);
}

struct block_device *blkdev_get_by_path(const char *path, fmode_t mode,
					void *holder)
{
	struct block_device *bdev;
	int fd, flags = O_DIRECT;

	if ((mode & (FMODE_READ|FMODE_WRITE)) == (FMODE_READ|FMODE_WRITE))
		flags = O_RDWR;
	else if (mode & FMODE_READ)
		flags = O_RDONLY;
	else if (mode & FMODE_WRITE)
		flags = O_WRONLY;

	if (mode & FMODE_EXCL)
		flags |= O_EXCL;

	fd = open(path, flags);
	if (fd < 0)
		return ERR_PTR(-errno);

	bdev = malloc(sizeof(*bdev));
	memset(bdev, 0, sizeof(*bdev));

	strncpy(bdev->name, path, sizeof(bdev->name));
	bdev->name[sizeof(bdev->name) - 1] = '\0';

	bdev->bd_fd	= fd;
	bdev->bd_holder = holder;
	bdev->bd_disk	= &bdev->__bd_disk;

	return bdev;
}

void bdput(struct block_device *bdev)
{
	BUG();
}

struct block_device *lookup_bdev(const char *path)
{
	return ERR_PTR(-EINVAL);
}
