/*
 * Copyright (C) 2001 Jens Axboe <axboe@kernel.dk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public Licens
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-
 *
 */
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/export.h>

void bio_copy_data_iter(struct bio *dst, struct bvec_iter *dst_iter,
			struct bio *src, struct bvec_iter *src_iter)
{
	struct bio_vec src_bv, dst_bv;
	void *src_p, *dst_p;
	unsigned bytes;

	while (src_iter->bi_size && dst_iter->bi_size) {
		src_bv = bio_iter_iovec(src, *src_iter);
		dst_bv = bio_iter_iovec(dst, *dst_iter);

		bytes = min(src_bv.bv_len, dst_bv.bv_len);

		src_p = kmap_atomic(src_bv.bv_page);
		dst_p = kmap_atomic(dst_bv.bv_page);

		memcpy(dst_p + dst_bv.bv_offset,
		       src_p + src_bv.bv_offset,
		       bytes);

		kunmap_atomic(dst_p);
		kunmap_atomic(src_p);

		flush_dcache_page(dst_bv.bv_page);

		bio_advance_iter(src, src_iter, bytes);
		bio_advance_iter(dst, dst_iter, bytes);
	}
}

/**
 * bio_copy_data - copy contents of data buffers from one bio to another
 * @src: source bio
 * @dst: destination bio
 *
 * Stops when it reaches the end of either @src or @dst - that is, copies
 * min(src->bi_size, dst->bi_size) bytes (or the equivalent for lists of bios).
 */
void bio_copy_data(struct bio *dst, struct bio *src)
{
	struct bvec_iter src_iter = src->bi_iter;
	struct bvec_iter dst_iter = dst->bi_iter;

	bio_copy_data_iter(dst, &dst_iter, src, &src_iter);
}

void zero_fill_bio_iter(struct bio *bio, struct bvec_iter start)
{
	unsigned long flags;
	struct bio_vec bv;
	struct bvec_iter iter;

	__bio_for_each_segment(bv, bio, iter, start) {
		char *data = bvec_kmap_irq(&bv, &flags);
		memset(data, 0, bv.bv_len);
		bvec_kunmap_irq(data, &flags);
	}
}

void __bio_clone_fast(struct bio *bio, struct bio *bio_src)
{
	/*
	 * most users will be overriding ->bi_bdev with a new target,
	 * so we don't set nor calculate new physical/hw segment counts here
	 */
	bio->bi_bdev = bio_src->bi_bdev;
	bio_set_flag(bio, BIO_CLONED);
	bio->bi_opf = bio_src->bi_opf;
	bio->bi_iter = bio_src->bi_iter;
	bio->bi_io_vec = bio_src->bi_io_vec;
}

struct bio *bio_clone_fast(struct bio *bio, gfp_t gfp_mask, struct bio_set *bs)
{
	struct bio *b;

	b = bio_alloc_bioset(gfp_mask, 0, bs);
	if (!b)
		return NULL;

	__bio_clone_fast(b, bio);
	return b;
}

struct bio *bio_split(struct bio *bio, int sectors,
		      gfp_t gfp, struct bio_set *bs)
{
	struct bio *split = NULL;

	BUG_ON(sectors <= 0);
	BUG_ON(sectors >= bio_sectors(bio));

	/*
	 * Discards need a mutable bio_vec to accommodate the payload
	 * required by the DSM TRIM and UNMAP commands.
	 */
	if (bio_op(bio) == REQ_OP_DISCARD || bio_op(bio) == REQ_OP_SECURE_ERASE)
		split = bio_clone_bioset(bio, gfp, bs);
	else
		split = bio_clone_fast(bio, gfp, bs);

	if (!split)
		return NULL;

	split->bi_iter.bi_size = sectors << 9;

	bio_advance(bio, split->bi_iter.bi_size);

	return split;
}

int bio_alloc_pages(struct bio *bio, gfp_t gfp_mask)
{
	int i;
	struct bio_vec *bv;

	bio_for_each_segment_all(bv, bio, i) {
		bv->bv_page = alloc_page(gfp_mask);
		if (!bv->bv_page) {
			while (--bv >= bio->bi_io_vec)
				__free_page(bv->bv_page);
			return -ENOMEM;
		}
	}

	return 0;
}

void bio_advance(struct bio *bio, unsigned bytes)
{
	bio_advance_iter(bio, &bio->bi_iter, bytes);
}

static void bio_free(struct bio *bio)
{
	unsigned front_pad = bio->bi_pool ? bio->bi_pool->front_pad : 0;

	kfree((void *) bio - front_pad);
}

void bio_put(struct bio *bio)
{
	if (!bio_flagged(bio, BIO_REFFED))
		bio_free(bio);
	else {
		BUG_ON(!atomic_read(&bio->__bi_cnt));

		/*
		 * last put frees it
		 */
		if (atomic_dec_and_test(&bio->__bi_cnt))
			bio_free(bio);
	}
}

static inline bool bio_remaining_done(struct bio *bio)
{
	/*
	 * If we're not chaining, then ->__bi_remaining is always 1 and
	 * we always end io on the first invocation.
	 */
	if (!bio_flagged(bio, BIO_CHAIN))
		return true;

	BUG_ON(atomic_read(&bio->__bi_remaining) <= 0);

	if (atomic_dec_and_test(&bio->__bi_remaining)) {
		bio_clear_flag(bio, BIO_CHAIN);
		return true;
	}

	return false;
}

static struct bio *__bio_chain_endio(struct bio *bio)
{
	struct bio *parent = bio->bi_private;

	if (!parent->bi_error)
		parent->bi_error = bio->bi_error;
	bio_put(bio);
	return parent;
}

static void bio_chain_endio(struct bio *bio)
{
	bio_endio(__bio_chain_endio(bio));
}

void bio_endio(struct bio *bio)
{
again:
	if (!bio_remaining_done(bio))
		return;

	/*
	 * Need to have a real endio function for chained bios, otherwise
	 * various corner cases will break (like stacking block devices that
	 * save/restore bi_end_io) - however, we want to avoid unbounded
	 * recursion and blowing the stack. Tail call optimization would
	 * handle this, but compiling with frame pointers also disables
	 * gcc's sibling call optimization.
	 */
	if (bio->bi_end_io == bio_chain_endio) {
		bio = __bio_chain_endio(bio);
		goto again;
	}

	if (bio->bi_end_io)
		bio->bi_end_io(bio);
}

void bio_endio_nodec(struct bio *bio)
{
	goto nodec;

	while (bio) {
		if (unlikely(!bio_remaining_done(bio)))
			break;
nodec:
		if (bio->bi_end_io == bio_chain_endio) {
			struct bio *parent = bio->bi_private;
			parent->bi_error = bio->bi_error;
			bio_put(bio);
			bio = parent;
		} else {
			if (bio->bi_end_io)
				bio->bi_end_io(bio);
			bio = NULL;
		}
	}
}

void bio_reset(struct bio *bio)
{
	unsigned long flags = bio->bi_flags & (~0UL << BIO_RESET_BITS);

	memset(bio, 0, BIO_RESET_BYTES);
	bio->bi_flags = flags;
	atomic_set(&bio->__bi_remaining, 1);
}

struct bio *bio_alloc_bioset(gfp_t gfp_mask, int nr_iovecs, struct bio_set *bs)
{
	unsigned front_pad = bs ? bs->front_pad : 0;
	struct bio *bio;
	void *p;

	p = kmalloc(front_pad +
		    sizeof(struct bio) +
		    nr_iovecs * sizeof(struct bio_vec),
		    gfp_mask);

	if (unlikely(!p))
		return NULL;

	bio = p + front_pad;
	bio_init(bio);
	bio->bi_pool		= bs;
	bio->bi_max_vecs	= nr_iovecs;
	bio->bi_io_vec		= bio->bi_inline_vecs;

	return bio;
}

struct bio *bio_clone_bioset(struct bio *bio_src, gfp_t gfp_mask,
			     struct bio_set *bs)
{
	struct bvec_iter iter;
	struct bio_vec bv;
	struct bio *bio;

	bio = bio_alloc_bioset(gfp_mask, bio_segments(bio_src), bs);
	if (!bio)
		return NULL;

	bio->bi_bdev		= bio_src->bi_bdev;
	bio->bi_opf		= bio_src->bi_opf;
	bio->bi_iter.bi_sector	= bio_src->bi_iter.bi_sector;
	bio->bi_iter.bi_size	= bio_src->bi_iter.bi_size;

	switch (bio_op(bio)) {
	case REQ_OP_DISCARD:
	case REQ_OP_SECURE_ERASE:
		break;
	case REQ_OP_WRITE_SAME:
		bio->bi_io_vec[bio->bi_vcnt++] = bio_src->bi_io_vec[0];
		break;
	default:
		bio_for_each_segment(bv, bio_src, iter)
			bio->bi_io_vec[bio->bi_vcnt++] = bv;
		break;
	}

	return bio;
}
