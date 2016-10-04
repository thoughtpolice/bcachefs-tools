/*
 * Cryptographic API for algorithms (i.e., low-level API).
 *
 * Copyright (c) 2006 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <crypto/algapi.h>
#include "internal.h"

static LIST_HEAD(crypto_alg_list);
static DECLARE_RWSEM(crypto_alg_sem);

static unsigned crypto_ctxsize(struct crypto_alg *alg, u32 type, u32 mask)
{
	return alg->cra_type->ctxsize(alg, type, mask);
}

unsigned crypto_alg_extsize(struct crypto_alg *alg)
{
	return alg->cra_ctxsize;
}

struct crypto_alg *crypto_alg_mod_lookup(const char *name, u32 type, u32 mask)
{
	struct crypto_alg *alg;

	down_read(&crypto_alg_sem);
	list_for_each_entry(alg, &crypto_alg_list, cra_list)
		if (!((alg->cra_flags ^ type) & mask) &&
		    !strcmp(alg->cra_name, name))
			goto found;

	alg = ERR_PTR(-ENOENT);
found:
	up_read(&crypto_alg_sem);

	return alg;
}

static void crypto_exit_ops(struct crypto_tfm *tfm)
{
	if (tfm->exit)
		tfm->exit(tfm);
}

static struct crypto_tfm *__crypto_alloc_tfm(struct crypto_alg *alg,
					     u32 type, u32 mask)
{
	struct crypto_tfm *tfm = NULL;
	unsigned tfm_size;
	int err = -ENOMEM;

	tfm_size = sizeof(*tfm) + crypto_ctxsize(alg, type, mask);
	tfm = kzalloc(tfm_size, GFP_KERNEL);
	if (tfm == NULL)
		return ERR_PTR(-ENOMEM);

	tfm->__crt_alg = alg;

	err = alg->cra_type->init(tfm, type, mask);
	if (err)
		goto out_free_tfm;

	if (!tfm->exit && alg->cra_init && (err = alg->cra_init(tfm)))
		goto cra_init_failed;

	return tfm;

cra_init_failed:
	crypto_exit_ops(tfm);
out_free_tfm:
	kfree(tfm);
	return ERR_PTR(err);
}

struct crypto_tfm *crypto_alloc_base(const char *alg_name, u32 type, u32 mask)
{
	struct crypto_alg *alg;
	struct crypto_tfm *tfm;

	alg = crypto_alg_mod_lookup(alg_name, type, mask);
	if (IS_ERR(alg)) {
		fprintf(stderr, "unknown cipher %s\n", alg_name);
		return ERR_CAST(alg);
	}

	tfm = __crypto_alloc_tfm(alg, type, mask);
	if (IS_ERR(tfm))
		return tfm;

	return tfm;
}

static void *crypto_create_tfm(struct crypto_alg *alg,
			       const struct crypto_type *frontend)
{
	struct crypto_tfm *tfm = NULL;
	unsigned tfmsize;
	unsigned total;
	void *mem;
	int err = -ENOMEM;

	tfmsize = frontend->tfmsize;
	total = tfmsize + sizeof(*tfm) + frontend->extsize(alg);

	mem = kzalloc(total, GFP_KERNEL);
	if (!mem)
		goto out_err;

	tfm = mem + tfmsize;
	tfm->__crt_alg = alg;

	err = frontend->init_tfm(tfm);
	if (err)
		goto out_free_tfm;

	if (!tfm->exit && alg->cra_init && (err = alg->cra_init(tfm)))
		goto cra_init_failed;

	goto out;

cra_init_failed:
	crypto_exit_ops(tfm);
out_free_tfm:
	kfree(mem);
out_err:
	mem = ERR_PTR(err);
out:
	return mem;
}

static struct crypto_alg *crypto_find_alg(const char *alg_name,
					  const struct crypto_type *frontend,
					  u32 type, u32 mask)
{
	if (frontend) {
		type &= frontend->maskclear;
		mask &= frontend->maskclear;
		type |= frontend->type;
		mask |= frontend->maskset;
	}

	return crypto_alg_mod_lookup(alg_name, type, mask);
}

void *crypto_alloc_tfm(const char *alg_name,
		       const struct crypto_type *frontend,
		       u32 type, u32 mask)
{
	struct crypto_alg *alg;
	void *tfm;

	alg = crypto_find_alg(alg_name, frontend, type, mask);
	if (IS_ERR(alg))
		return ERR_CAST(alg);

	tfm = crypto_create_tfm(alg, frontend);
	if (IS_ERR(tfm))
		return tfm;

	return tfm;
}

void crypto_destroy_tfm(void *mem, struct crypto_tfm *tfm)
{
	struct crypto_alg *alg;

	if (unlikely(!mem))
		return;

	alg = tfm->__crt_alg;

	if (!tfm->exit && alg->cra_exit)
		alg->cra_exit(tfm);
	crypto_exit_ops(tfm);
	kzfree(mem);
}

int crypto_register_alg(struct crypto_alg *alg)
{
	INIT_LIST_HEAD(&alg->cra_users);

	down_write(&crypto_alg_sem);
	list_add(&alg->cra_list, &crypto_alg_list);
	up_write(&crypto_alg_sem);

	return 0;
}
