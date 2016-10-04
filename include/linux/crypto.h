/*
 * Scatterlist Cryptographic API.
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2002 David S. Miller (davem@redhat.com)
 * Copyright (c) 2005 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * Portions derived from Cryptoapi, by Alexander Kjeldaas <astor@fast.no>
 * and Nettle, by Niels Möller.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */
#ifndef _LINUX_CRYPTO_H
#define _LINUX_CRYPTO_H

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>

#define CRYPTO_ALG_TYPE_MASK		0x0000000f
#define CRYPTO_ALG_TYPE_BLKCIPHER	0x00000004
#define CRYPTO_ALG_TYPE_SHASH		0x0000000e
#define CRYPTO_ALG_TYPE_BLKCIPHER_MASK	0x0000000c
#define CRYPTO_ALG_ASYNC		0x00000080

#define CRYPTO_MAX_ALG_NAME		64

#define CRYPTO_MINALIGN ARCH_KMALLOC_MINALIGN
#define CRYPTO_MINALIGN_ATTR __attribute__ ((__aligned__(CRYPTO_MINALIGN)))

struct scatterlist;
struct crypto_blkcipher;
struct crypto_tfm;
struct crypto_type;

struct blkcipher_desc {
	struct crypto_blkcipher	*tfm;
	void			*info;
	u32			flags;
};

struct blkcipher_alg {
	int (*setkey)(struct crypto_tfm *tfm, const u8 *key,
		      unsigned keylen);
	int (*encrypt)(struct blkcipher_desc *desc,
		       struct scatterlist *dst, struct scatterlist *src,
		       unsigned nbytes);
	int (*decrypt)(struct blkcipher_desc *desc,
		       struct scatterlist *dst, struct scatterlist *src,
		       unsigned nbytes);
};

#define cra_blkcipher	cra_u.blkcipher

struct crypto_alg {
	struct list_head	cra_list;
	struct list_head	cra_users;

	u32			cra_flags;
	unsigned		cra_ctxsize;
	char			cra_name[CRYPTO_MAX_ALG_NAME];

	const struct crypto_type *cra_type;

	union {
		struct blkcipher_alg blkcipher;
	} cra_u;

	int (*cra_init)(struct crypto_tfm *tfm);
	void (*cra_exit)(struct crypto_tfm *tfm);
} CRYPTO_MINALIGN_ATTR;

int crypto_register_alg(struct crypto_alg *alg);

struct blkcipher_tfm {
	int (*setkey)(struct crypto_tfm *tfm, const u8 *key,
		      unsigned keylen);
	int (*encrypt)(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned nbytes);
	int (*decrypt)(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned nbytes);
};

struct crypto_tfm {
	u32			crt_flags;

	struct blkcipher_tfm	crt_blkcipher;

	void (*exit)(struct crypto_tfm *tfm);

	struct crypto_alg	*__crt_alg;
	void			*__crt_ctx[] CRYPTO_MINALIGN_ATTR;
};

struct crypto_tfm *crypto_alloc_base(const char *alg_name, u32 type, u32 mask);
void crypto_destroy_tfm(void *mem, struct crypto_tfm *tfm);

static inline void crypto_free_tfm(struct crypto_tfm *tfm)
{
	return crypto_destroy_tfm(tfm, tfm);
}

static inline u32 crypto_tfm_alg_type(struct crypto_tfm *tfm)
{
	return tfm->__crt_alg->cra_flags & CRYPTO_ALG_TYPE_MASK;
}

static inline void *crypto_tfm_ctx(struct crypto_tfm *tfm)
{
	return tfm->__crt_ctx;
}

struct crypto_blkcipher {
	struct crypto_tfm base;
};

static inline struct crypto_blkcipher *__crypto_blkcipher_cast(
	struct crypto_tfm *tfm)
{
	return (struct crypto_blkcipher *)tfm;
}

static inline struct crypto_blkcipher *crypto_blkcipher_cast(
	struct crypto_tfm *tfm)
{
	BUG_ON(crypto_tfm_alg_type(tfm) != CRYPTO_ALG_TYPE_BLKCIPHER);
	return __crypto_blkcipher_cast(tfm);
}

static inline struct crypto_blkcipher *crypto_alloc_blkcipher(
	const char *alg_name, u32 type, u32 mask)
{
	type &= ~CRYPTO_ALG_TYPE_MASK;
	type |= CRYPTO_ALG_TYPE_BLKCIPHER;
	mask |= CRYPTO_ALG_TYPE_MASK;

	return __crypto_blkcipher_cast(crypto_alloc_base(alg_name, type, mask));
}

static inline void crypto_free_blkcipher(struct crypto_blkcipher *tfm)
{
	crypto_free_tfm(&tfm->base);
}

static inline struct blkcipher_tfm *crypto_blkcipher_crt(
	struct crypto_blkcipher *tfm)
{
	return &tfm->base.crt_blkcipher;
}

static inline int crypto_blkcipher_setkey(struct crypto_blkcipher *tfm,
					  const u8 *key, unsigned keylen)
{
	return crypto_blkcipher_crt(tfm)->setkey(&tfm->base, key, keylen);
}

static inline int crypto_blkcipher_encrypt_iv(struct blkcipher_desc *desc,
					      struct scatterlist *dst,
					      struct scatterlist *src,
					      unsigned nbytes)
{
	return crypto_blkcipher_crt(desc->tfm)->encrypt(desc, dst, src, nbytes);
}

#endif	/* _LINUX_CRYPTO_H */

