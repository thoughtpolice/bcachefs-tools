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
#include <linux/kernel.h>
#include <linux/slab.h>

struct skcipher_request {
	unsigned int cryptlen;

	u8 *iv;

	struct scatterlist *src;
	struct scatterlist *dst;

	struct crypto_tfm *tfm;
	//struct crypto_async_request base;

	void *__ctx[] CRYPTO_MINALIGN_ATTR;
};

struct crypto_skcipher {
	int (*setkey)(struct crypto_skcipher *tfm, const u8 *key,
	              unsigned int keylen);
	int (*encrypt)(struct skcipher_request *req);
	int (*decrypt)(struct skcipher_request *req);

	unsigned int ivsize;
	unsigned int reqsize;
	unsigned int keysize;

	struct crypto_tfm base;
};

struct skcipher_alg {
	int (*setkey)(struct crypto_skcipher *tfm, const u8 *key,
	              unsigned int keylen);
	int (*encrypt)(struct skcipher_request *req);
	int (*decrypt)(struct skcipher_request *req);
	int (*init)(struct crypto_skcipher *tfm);
	void (*exit)(struct crypto_skcipher *tfm);

	unsigned int min_keysize;
	unsigned int max_keysize;
	unsigned int ivsize;
	unsigned int chunksize;
	unsigned int walksize;

	struct crypto_alg base;
};

#define SKCIPHER_REQUEST_ON_STACK(name, tfm) \
	char __##name##_desc[sizeof(struct skcipher_request) + \
		crypto_skcipher_reqsize(tfm)] CRYPTO_MINALIGN_ATTR; \
	struct skcipher_request *name = (void *)__##name##_desc

static inline void *crypto_skcipher_ctx(struct crypto_skcipher *tfm)
{
	return crypto_tfm_ctx(&tfm->base);
}

static inline struct crypto_skcipher *__crypto_skcipher_cast(
	struct crypto_tfm *tfm)
{
	return container_of(tfm, struct crypto_skcipher, base);
}

struct crypto_skcipher *crypto_alloc_skcipher(const char *alg_name,
					      u32 type, u32 mask);

static inline struct crypto_tfm *crypto_skcipher_tfm(
	struct crypto_skcipher *tfm)
{
	return &tfm->base;
}

static inline void crypto_free_skcipher(struct crypto_skcipher *tfm)
{
	crypto_destroy_tfm(tfm, crypto_skcipher_tfm(tfm));
}

static inline struct skcipher_alg *crypto_skcipher_alg(
	struct crypto_skcipher *tfm)
{
	return container_of(crypto_skcipher_tfm(tfm)->__crt_alg,
			    struct skcipher_alg, base);
}

static inline int crypto_skcipher_setkey(struct crypto_skcipher *tfm,
					 const u8 *key, unsigned int keylen)
{
	return tfm->setkey(tfm, key, keylen);
}

static inline struct crypto_skcipher *crypto_skcipher_reqtfm(
	struct skcipher_request *req)
{
	return __crypto_skcipher_cast(req->tfm);
}

static inline int crypto_skcipher_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);

	return tfm->encrypt(req);
}

static inline int crypto_skcipher_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);

	return tfm->decrypt(req);
}

static inline unsigned int crypto_skcipher_reqsize(struct crypto_skcipher *tfm)
{
	return tfm->reqsize;
}

static inline void skcipher_request_set_tfm(struct skcipher_request *req,
					    struct crypto_skcipher *tfm)
{
	req->tfm = crypto_skcipher_tfm(tfm);
}

static inline void skcipher_request_set_crypt(
	struct skcipher_request *req,
	struct scatterlist *src, struct scatterlist *dst,
	unsigned int cryptlen, void *iv)
{
	req->src = src;
	req->dst = dst;
	req->cryptlen = cryptlen;
	req->iv = iv;
}

#endif	/* _CRYPTO_SKCIPHER_H */
