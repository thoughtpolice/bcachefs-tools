/*
 * Cryptographic API.
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2005 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#ifndef _CRYPTO_INTERNAL_H
#define _CRYPTO_INTERNAL_H

struct crypto_type;
struct crypto_alg;

void *crypto_alloc_tfm(const char *, const struct crypto_type *, u32, u32);
unsigned int crypto_alg_extsize(struct crypto_alg *);

#endif	/* _CRYPTO_INTERNAL_H */

