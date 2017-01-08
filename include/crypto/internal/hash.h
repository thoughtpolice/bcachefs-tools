#ifndef _CRYPTO_INTERNAL_HASH_H
#define _CRYPTO_INTERNAL_HASH_H

#include <crypto/algapi.h>
#include <crypto/hash.h>

int crypto_register_shash(struct shash_alg *alg);
int crypto_unregister_shash(struct shash_alg *alg);
int crypto_register_shashes(struct shash_alg *algs, int count);
int crypto_unregister_shashes(struct shash_alg *algs, int count);

static inline struct crypto_shash *__crypto_shash_cast(struct crypto_tfm *tfm)
{
	return container_of(tfm, struct crypto_shash, base);
}

#endif	/* _CRYPTO_INTERNAL_HASH_H */

