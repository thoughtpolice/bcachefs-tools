/*
 * Synchronous Cryptographic Hash operations.
 *
 * Copyright (c) 2008 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#include <crypto/internal/hash.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>

#include "internal.h"

static int shash_finup(struct shash_desc *desc, const u8 *data,
		       unsigned len, u8 *out)
{
	return crypto_shash_update(desc, data, len) ?:
	       crypto_shash_final(desc, out);
}

static int shash_digest(struct shash_desc *desc, const u8 *data,
				  unsigned len, u8 *out)
{
	return crypto_shash_init(desc) ?:
	       crypto_shash_finup(desc, data, len, out);
}

static int crypto_shash_init_tfm(struct crypto_tfm *tfm)
{
	struct crypto_shash *hash = __crypto_shash_cast(tfm);

	hash->descsize = crypto_shash_alg(hash)->descsize;
	return 0;
}

static const struct crypto_type crypto_shash_type = {
	.extsize	= crypto_alg_extsize,
	.init_tfm	= crypto_shash_init_tfm,
	.maskclear	= ~CRYPTO_ALG_TYPE_MASK,
	.maskset	= CRYPTO_ALG_TYPE_MASK,
	.type		= CRYPTO_ALG_TYPE_SHASH,
	.tfmsize	= offsetof(struct crypto_shash, base),
};

struct crypto_shash *crypto_alloc_shash(const char *alg_name,
					u32 type, u32 mask)
{
	return crypto_alloc_tfm(alg_name, &crypto_shash_type, type, mask);
}

int crypto_register_shash(struct shash_alg *alg)
{
	struct crypto_alg *base = &alg->base;

	base->cra_type = &crypto_shash_type;
	base->cra_flags &= ~CRYPTO_ALG_TYPE_MASK;
	base->cra_flags |= CRYPTO_ALG_TYPE_SHASH;

	if (!alg->finup)
		alg->finup = shash_finup;
	if (!alg->digest)
		alg->digest = shash_digest;

	return crypto_register_alg(base);
}
