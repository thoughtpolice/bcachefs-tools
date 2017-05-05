/*
 * ChaCha20 256-bit cipher algorithm, RFC7539
 *
 * Copyright (C) 2015 Martin Willi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/byteorder.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <asm/unaligned.h>

#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/chacha20.h>
#include <crypto/skcipher.h>

#include <sodium/crypto_stream_chacha20.h>

struct chacha20_ctx {
	u32 key[8];
};

static int crypto_chacha20_setkey(struct crypto_skcipher *tfm, const u8 *key,
				  unsigned int keysize)
{
	struct chacha20_ctx *ctx = crypto_skcipher_ctx(tfm);
	int i;

	if (keysize != CHACHA20_KEY_SIZE)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(ctx->key); i++)
		ctx->key[i] = get_unaligned_le32(key + i * sizeof(u32));

	return 0;
}

static int crypto_chacha20_crypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct chacha20_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct scatterlist *sg = req->src;
	unsigned nbytes = req->cryptlen;
	u32 iv[4];
	int ret;

	BUG_ON(req->src != req->dst);

	memcpy(iv, req->iv, sizeof(iv));

	while (1) {
		ret = crypto_stream_chacha20_xor_ic(sg_virt(sg),
						    sg_virt(sg),
						    sg->length,
						    (void *) &iv[2],
						    iv[0] | ((u64) iv[1] << 32),
						    (void *) ctx->key);
		BUG_ON(ret);

		nbytes -= sg->length;

		if (sg_is_last(sg))
			break;

		BUG_ON(sg->length % CHACHA20_BLOCK_SIZE);
		iv[0] += sg->length / CHACHA20_BLOCK_SIZE;
		sg = sg_next(sg);
	};

	BUG_ON(nbytes);

	return 0;
}

static struct skcipher_alg alg = {
	.base.cra_name		= "chacha20",
	.base.cra_ctxsize	= sizeof(struct chacha20_ctx),

	.min_keysize		= CHACHA20_KEY_SIZE,
	.max_keysize		= CHACHA20_KEY_SIZE,
	.ivsize			= CHACHA20_IV_SIZE,
	.chunksize		= CHACHA20_BLOCK_SIZE,
	.setkey			= crypto_chacha20_setkey,
	.encrypt		= crypto_chacha20_crypt,
	.decrypt		= crypto_chacha20_crypt,
};

__attribute__((constructor(110)))
static int chacha20_generic_mod_init(void)
{
	return crypto_register_alg(&alg.base);
}
