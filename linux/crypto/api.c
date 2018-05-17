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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <crypto/algapi.h>

static LIST_HEAD(crypto_alg_list);
static DECLARE_RWSEM(crypto_alg_sem);

struct crypto_type {
};

int crypto_register_alg(struct crypto_alg *alg)
{
	down_write(&crypto_alg_sem);
	list_add(&alg->cra_list, &crypto_alg_list);
	up_write(&crypto_alg_sem);

	return 0;
}

static void *crypto_alloc_tfm(const char *name,
			      const struct crypto_type *type)
{
	struct crypto_alg *alg;

	down_read(&crypto_alg_sem);
	list_for_each_entry(alg, &crypto_alg_list, cra_list)
		if (alg->cra_type == type && !strcmp(alg->cra_name, name))
			goto found;

	alg = ERR_PTR(-ENOENT);
found:
	up_read(&crypto_alg_sem);

	if (IS_ERR(alg))
		return ERR_CAST(alg);

	return alg->alloc_tfm() ?: ERR_PTR(-ENOMEM);
}

/* skcipher: */

static const struct crypto_type crypto_skcipher_type2 = {
};

struct crypto_skcipher *crypto_alloc_skcipher(const char *name,
					      u32 type, u32 mask)
{
	return crypto_alloc_tfm(name, &crypto_skcipher_type2);
}

int crypto_register_skcipher(struct skcipher_alg *alg)
{
	alg->base.cra_type = &crypto_skcipher_type2;

	return crypto_register_alg(&alg->base);
}

/* shash: */

#include <crypto/hash.h>

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

static const struct crypto_type crypto_shash_type = {
};

struct crypto_shash *crypto_alloc_shash(const char *name,
					u32 type, u32 mask)
{
	return crypto_alloc_tfm(name, &crypto_shash_type);
}

int crypto_register_shash(struct shash_alg *alg)
{
	alg->base.cra_type = &crypto_shash_type;

	if (!alg->finup)
		alg->finup = shash_finup;
	if (!alg->digest)
		alg->digest = shash_digest;

	return crypto_register_alg(&alg->base);
}
