#ifndef _CRYPTO_H
#define _CRYPTO_H

#include "util.h"

struct bcache_key {
	u64	key[4];
};

struct bcache_disk_key {
	u64	header;
	u64	key[4];
};

static const char bch_key_header[8]		= BCACHE_MASTER_KEY_HEADER;
static const struct nonce bch_master_key_nonce	= BCACHE_MASTER_KEY_NONCE;

char *read_passphrase(const char *);
void derive_passphrase(struct bcache_key *, const char *);
void disk_key_encrypt(struct cache_sb *sb, struct bcache_disk_key *,
		      struct bcache_key *);
void disk_key_init(struct bcache_disk_key *);

#endif /* _CRYPTO_H */
