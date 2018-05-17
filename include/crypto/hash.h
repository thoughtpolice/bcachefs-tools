/*
 * Hash: Hash algorithms under the crypto API
 * 
 * Copyright (c) 2008 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */

#ifndef _CRYPTO_HASH_H
#define _CRYPTO_HASH_H

#include <linux/crypto.h>

struct shash_desc;

struct shash_alg {
	int (*init)(struct shash_desc *desc);
	int (*update)(struct shash_desc *desc, const u8 *data, unsigned len);
	int (*final)(struct shash_desc *desc, u8 *out);
	int (*finup)(struct shash_desc *desc, const u8 *data,
		     unsigned len, u8 *out);
	int (*digest)(struct shash_desc *desc, const u8 *data,
		      unsigned len, u8 *out);

	unsigned		descsize;
	unsigned		digestsize;
	struct crypto_alg	base;
};

int crypto_register_shash(struct shash_alg *alg);

struct crypto_shash {
	unsigned		descsize;
	struct crypto_tfm	base;
};

struct crypto_shash *crypto_alloc_shash(const char *alg_name, u32 type,
					u32 mask);

static inline void crypto_free_shash(struct crypto_shash *tfm)
{
	kfree(tfm);
}

static inline struct shash_alg *crypto_shash_alg(struct crypto_shash *tfm)
{
	return container_of(tfm->base.alg, struct shash_alg, base);
}

static inline unsigned crypto_shash_digestsize(struct crypto_shash *tfm)
{
	return crypto_shash_alg(tfm)->digestsize;
}

static inline unsigned crypto_shash_descsize(struct crypto_shash *tfm)
{
	return tfm->descsize;
}

struct shash_desc {
	struct crypto_shash	*tfm;
	u32			flags;

	void			*ctx[] CRYPTO_MINALIGN_ATTR;
};

#define SHASH_DESC_ON_STACK(shash, tfm)				  \
	char __##shash##_desc[sizeof(struct shash_desc) +	  \
		crypto_shash_descsize(tfm)] CRYPTO_MINALIGN_ATTR; \
	struct shash_desc *shash = (struct shash_desc *)__##shash##_desc

static inline int crypto_shash_init(struct shash_desc *desc)
{
	return crypto_shash_alg(desc->tfm)->init(desc);
}

static inline int crypto_shash_update(struct shash_desc *desc,
				      const u8 *data, unsigned len)
{
	return crypto_shash_alg(desc->tfm)->update(desc, data, len);
}

static inline int crypto_shash_final(struct shash_desc *desc, u8 *out)
{
	return crypto_shash_alg(desc->tfm)->final(desc, out);
}

static inline int crypto_shash_finup(struct shash_desc *desc, const u8 *data,
				     unsigned len, u8 *out)
{
	return crypto_shash_alg(desc->tfm)->finup(desc, data, len, out);
}

static inline int crypto_shash_digest(struct shash_desc *desc, const u8 *data,
				      unsigned len, u8 *out)
{
	return crypto_shash_alg(desc->tfm)->digest(desc, data, len, out);
}

#endif	/* _CRYPTO_HASH_H */
