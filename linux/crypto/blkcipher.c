/*
 * Block chaining cipher operations.
 *
 * Generic encrypt/decrypt wrapper for ciphers, handles operations across
 * multiple page boundaries by using temporary blocks.  In user context,
 * the kernel is given a chance to schedule us once per page.
 *
 * Copyright (c) 2006 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <crypto/algapi.h>
#include "internal.h"

static unsigned crypto_blkcipher_ctxsize(struct crypto_alg *alg,
					 u32 type, u32 mask)
{
	return alg->cra_ctxsize;
}

static int crypto_init_blkcipher_ops(struct crypto_tfm *tfm, u32 type, u32 mask)
{
	struct blkcipher_tfm *crt = &tfm->crt_blkcipher;
	struct blkcipher_alg *alg = &tfm->__crt_alg->cra_blkcipher;

	BUG_ON((mask & CRYPTO_ALG_TYPE_MASK) != CRYPTO_ALG_TYPE_MASK);

	crt->setkey	= alg->setkey;
	crt->encrypt	= alg->encrypt;
	crt->decrypt	= alg->decrypt;
	return 0;
}

const struct crypto_type crypto_blkcipher_type = {
	.ctxsize	= crypto_blkcipher_ctxsize,
	.init		= crypto_init_blkcipher_ops,
};
