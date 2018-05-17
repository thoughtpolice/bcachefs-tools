/*
 * Poly1305 authenticator algorithm, RFC7539
 *
 * Copyright (C) 2015 Martin Willi
 *
 * Based on public domain code by Andrew Moon and Daniel J. Bernstein.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/byteorder.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>

#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/poly1305.h>

static struct shash_alg poly1305_alg;

struct poly1305_desc_ctx {
	bool					key_done;
	crypto_onetimeauth_poly1305_state	s;
};

static int poly1305_init(struct shash_desc *desc)
{
	struct poly1305_desc_ctx *state = (void *) desc->ctx;

	state->key_done = false;
	return 0;
}

static int poly1305_update(struct shash_desc *desc,
			   const u8 *src, unsigned len)
{
	struct poly1305_desc_ctx *state = (void *) desc->ctx;

	if (!state->key_done) {
		BUG_ON(len != crypto_onetimeauth_poly1305_KEYBYTES);

		state->key_done = true;
		return crypto_onetimeauth_poly1305_init(&state->s, src);
	}

	return crypto_onetimeauth_poly1305_update(&state->s, src, len);
}

static int poly1305_final(struct shash_desc *desc, u8 *out)
{
	struct poly1305_desc_ctx *state = (void *) desc->ctx;

	return crypto_onetimeauth_poly1305_final(&state->s, out);
}

static void *poly1305_alloc_tfm(void)
{
	struct crypto_shash *tfm = kzalloc(sizeof(*tfm), GFP_KERNEL);

	if (!tfm)
		return NULL;

	tfm->base.alg = &poly1305_alg.base;
	tfm->descsize = sizeof(struct poly1305_desc_ctx);
	return tfm;
}

static struct shash_alg poly1305_alg = {
	.digestsize	= crypto_onetimeauth_poly1305_BYTES,
	.init		= poly1305_init,
	.update		= poly1305_update,
	.final		= poly1305_final,
	.descsize	= sizeof(struct poly1305_desc_ctx),

	.base.cra_name	= "poly1305",
	.base.alloc_tfm	= poly1305_alloc_tfm,
};

__attribute__((constructor(110)))
static int poly1305_mod_init(void)
{
	return crypto_register_shash(&poly1305_alg);
}
