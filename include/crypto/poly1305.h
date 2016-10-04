/*
 * Common values for the Poly1305 algorithm
 */

#ifndef _CRYPTO_POLY1305_H
#define _CRYPTO_POLY1305_H

#include <sodium/crypto_onetimeauth_poly1305.h>

#define POLY1305_KEY_SIZE	crypto_onetimeauth_poly1305_KEYBYTES
#define POLY1305_DIGEST_SIZE	crypto_onetimeauth_poly1305_BYTES

#endif
