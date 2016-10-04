#ifndef _CRYPTO_INTERNAL_HASH_H
#define _CRYPTO_INTERNAL_HASH_H

#include <crypto/algapi.h>
#include <crypto/hash.h>

int crypto_register_shash(struct shash_alg *alg);

static inline struct crypto_shash *__crypto_shash_cast(struct crypto_tfm *tfm)
{
	return container_of(tfm, struct crypto_shash, base);
}

#endif	/* _CRYPTO_INTERNAL_HASH_H */

