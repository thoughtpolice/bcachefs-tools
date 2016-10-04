/*
 * Cryptographic API for algorithms (i.e., low-level API).
 *
 * Copyright (c) 2006 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */
#ifndef _CRYPTO_ALGAPI_H
#define _CRYPTO_ALGAPI_H

#include <linux/crypto.h>

struct crypto_type {
	unsigned int (*ctxsize)(struct crypto_alg *alg, u32 type, u32 mask);
	unsigned int (*extsize)(struct crypto_alg *alg);
	int (*init)(struct crypto_tfm *tfm, u32 type, u32 mask);
	int (*init_tfm)(struct crypto_tfm *tfm);

	unsigned	type;
	unsigned	maskclear;
	unsigned	maskset;
	unsigned	tfmsize;
};

extern const struct crypto_type crypto_blkcipher_type;

static inline void *crypto_blkcipher_ctx(struct crypto_blkcipher *tfm)
{
	return crypto_tfm_ctx(&tfm->base);
}

#endif	/* _CRYPTO_ALGAPI_H */
