/*
 * Symmetric key ciphers.
 * 
 * Copyright (c) 2007-2015 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */

#ifndef _CRYPTO_SKCIPHER_H
#define _CRYPTO_SKCIPHER_H

#include <linux/crypto.h>

struct crypto_skcipher;
struct skcipher_request;

struct skcipher_alg {
	struct crypto_alg base;
};

int crypto_register_skcipher(struct skcipher_alg *alg);

struct crypto_skcipher {
	int (*setkey)(struct crypto_skcipher *tfm, const u8 *key,
	              unsigned int keylen);
	int (*encrypt)(struct skcipher_request *req);
	int (*decrypt)(struct skcipher_request *req);

	unsigned		ivsize;
	unsigned		keysize;

	struct crypto_tfm	base;
};

struct crypto_skcipher *crypto_alloc_skcipher(const char *alg_name,
					      u32 type, u32 mask);

static inline void crypto_free_skcipher(struct crypto_skcipher *tfm)
{
	kfree(tfm);
}

struct skcipher_request {
	unsigned		cryptlen;
	u8			*iv;

	struct scatterlist	*src;
	struct scatterlist	*dst;

	struct crypto_tfm	*tfm;
};

#define SKCIPHER_REQUEST_ON_STACK(name, tfm)			\
	struct skcipher_request __##name##_desc;		\
	struct skcipher_request *name = &__##name##_desc

static inline int crypto_skcipher_setkey(struct crypto_skcipher *tfm,
					 const u8 *key, unsigned int keylen)
{
	return tfm->setkey(tfm, key, keylen);
}

static inline struct crypto_skcipher *crypto_skcipher_reqtfm(
	struct skcipher_request *req)
{
	return container_of(req->tfm, struct crypto_skcipher, base);
}

static inline int crypto_skcipher_encrypt(struct skcipher_request *req)
{
	return crypto_skcipher_reqtfm(req)->encrypt(req);
}

static inline int crypto_skcipher_decrypt(struct skcipher_request *req)
{
	return crypto_skcipher_reqtfm(req)->decrypt(req);
}

static inline void skcipher_request_set_tfm(struct skcipher_request *req,
					    struct crypto_skcipher *tfm)
{
	req->tfm = &tfm->base;
}

static inline void skcipher_request_set_crypt(
	struct skcipher_request *req,
	struct scatterlist *src, struct scatterlist *dst,
	unsigned int cryptlen, void *iv)
{
	req->src	= src;
	req->dst	= dst;
	req->cryptlen	= cryptlen;
	req->iv		= iv;
}

#endif	/* _CRYPTO_SKCIPHER_H */
