/*
 * Cryptographic API.
 *
 * SHA-256, as specified in
 * http://csrc.nist.gov/groups/STM/cavp/documents/shs/sha256-384-512.pdf
 *
 * SHA-256 code by Jean-Luc Cooke <jlcooke@certainkey.com>.
 *
 * Copyright (c) Jean-Luc Cooke <jlcooke@certainkey.com>
 * Copyright (c) Andrew McDonald <andrew@mcdonald.org.uk>
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * SHA224 Support Copyright 2007 Intel Corporation <jonathan.lynch@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */

#include <linux/bitops.h>
#include <linux/byteorder.h>
#include <linux/types.h>
#include <asm/unaligned.h>

#include <linux/crypto.h>
#include <crypto/hash.h>

#include <sodium/crypto_hash_sha256.h>

static struct shash_alg sha256_alg;

static int sha256_init(struct shash_desc *desc)
{
	crypto_hash_sha256_state *state = (void *) desc->ctx;

	return crypto_hash_sha256_init(state);
}

static int sha256_update(struct shash_desc *desc, const u8 *data,
			  unsigned int len)
{
	crypto_hash_sha256_state *state = (void *) desc->ctx;

	return crypto_hash_sha256_update(state, data, len);
}

static int sha256_final(struct shash_desc *desc, u8 *out)
{
	crypto_hash_sha256_state *state = (void *) desc->ctx;

	return crypto_hash_sha256_final(state, out);
}

static void *sha256_alloc_tfm(void)
{
	struct crypto_shash *tfm = kzalloc(sizeof(*tfm), GFP_KERNEL);

	if (!tfm)
		return NULL;

	tfm->base.alg = &sha256_alg.base;
	tfm->descsize = sizeof(crypto_hash_sha256_state);
	return tfm;
}

static struct shash_alg sha256_alg = {
	.digestsize	= crypto_hash_sha256_BYTES,
	.init		= sha256_init,
	.update		= sha256_update,
	.final		= sha256_final,
	.descsize	= sizeof(crypto_hash_sha256_state),
	.base.cra_name	= "sha256",
	.base.alloc_tfm	= sha256_alloc_tfm,
};

__attribute__((constructor(110)))
static int __init sha256_generic_mod_init(void)
{
	return crypto_register_shash(&sha256_alg);
}
