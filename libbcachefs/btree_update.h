#ifndef _BCACHEFS_BTREE_UPDATE_H
#define _BCACHEFS_BTREE_UPDATE_H

#include "btree_iter.h"
#include "journal.h"

struct bch_fs;
struct btree;
struct btree_insert;

void bch2_btree_node_lock_for_insert(struct bch_fs *, struct btree *,
				     struct btree_iter *);
bool bch2_btree_bset_insert_key(struct btree_iter *, struct btree *,
				struct btree_node_iter *, struct bkey_i *);
void bch2_btree_journal_key(struct btree_insert *trans, struct btree_iter *,
			    struct bkey_i *);

/* Normal update interface: */

struct btree_insert {
	struct bch_fs		*c;
	struct disk_reservation *disk_res;
	struct journal_res	journal_res;
	u64			*journal_seq;
	struct extent_insert_hook *hook;
	unsigned		flags;
	bool			did_work;

	unsigned short		nr;
	struct btree_insert_entry  *entries;
};

int __bch2_btree_insert_at(struct btree_insert *);

#define _TENTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...)   N
#define COUNT_ARGS(...)  _TENTH_ARG(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define BTREE_INSERT_ENTRY(_iter, _k)					\
	((struct btree_insert_entry) {					\
		.iter		= (_iter),				\
		.k		= (_k),					\
		.done		= false,				\
	})

#define BTREE_INSERT_ENTRY_EXTRA_RES(_iter, _k, _extra)			\
	((struct btree_insert_entry) {					\
		.iter		= (_iter),				\
		.k		= (_k),					\
		.extra_res = (_extra),					\
		.done		= false,				\
	})

/**
 * bch_btree_insert_at - insert one or more keys at iterator positions
 * @iter:		btree iterator
 * @insert_key:		key to insert
 * @disk_res:		disk reservation
 * @hook:		extent insert callback
 *
 * Return values:
 * -EINTR: locking changed, this function should be called again. Only returned
 *  if passed BTREE_INSERT_ATOMIC.
 * -EROFS: filesystem read only
 * -EIO: journal or btree node IO error
 */
#define bch2_btree_insert_at(_c, _disk_res, _hook,			\
			    _journal_seq, _flags, ...)			\
	__bch2_btree_insert_at(&(struct btree_insert) {			\
		.c		= (_c),					\
		.disk_res	= (_disk_res),				\
		.journal_seq	= (_journal_seq),			\
		.hook		= (_hook),				\
		.flags		= (_flags),				\
		.nr		= COUNT_ARGS(__VA_ARGS__),		\
		.entries	= (struct btree_insert_entry[]) {	\
			__VA_ARGS__					\
		}})

enum {
	__BTREE_INSERT_ATOMIC,
	__BTREE_INSERT_NOUNLOCK,
	__BTREE_INSERT_NOFAIL,
	__BTREE_INSERT_USE_RESERVE,
	__BTREE_INSERT_USE_ALLOC_RESERVE,
	__BTREE_INSERT_JOURNAL_REPLAY,
	__BTREE_INSERT_NOWAIT,
	__BTREE_INSERT_GC_LOCK_HELD,
	__BCH_HASH_SET_MUST_CREATE,
	__BCH_HASH_SET_MUST_REPLACE,
};

/*
 * Don't drop/retake locks before doing btree update, instead return -EINTR if
 * we had to drop locks for any reason
 */
#define BTREE_INSERT_ATOMIC		(1 << __BTREE_INSERT_ATOMIC)

/*
 * Don't drop locks _after_ successfully updating btree:
 */
#define BTREE_INSERT_NOUNLOCK		(1 << __BTREE_INSERT_NOUNLOCK)

/* Don't check for -ENOSPC: */
#define BTREE_INSERT_NOFAIL		(1 << __BTREE_INSERT_NOFAIL)

/* for copygc, or when merging btree nodes */
#define BTREE_INSERT_USE_RESERVE	(1 << __BTREE_INSERT_USE_RESERVE)
#define BTREE_INSERT_USE_ALLOC_RESERVE	(1 << __BTREE_INSERT_USE_ALLOC_RESERVE)

/*
 * Insert is for journal replay: don't get journal reservations, or mark extents
 * (bch_mark_key)
 */
#define BTREE_INSERT_JOURNAL_REPLAY	(1 << __BTREE_INSERT_JOURNAL_REPLAY)

/* Don't block on allocation failure (for new btree nodes: */
#define BTREE_INSERT_NOWAIT		(1 << __BTREE_INSERT_NOWAIT)
#define BTREE_INSERT_GC_LOCK_HELD	(1 << __BTREE_INSERT_GC_LOCK_HELD)

#define BCH_HASH_SET_MUST_CREATE	(1 << __BCH_HASH_SET_MUST_CREATE)
#define BCH_HASH_SET_MUST_REPLACE	(1 << __BCH_HASH_SET_MUST_REPLACE)

int bch2_btree_delete_at(struct btree_iter *, unsigned);

int bch2_btree_insert_list_at(struct btree_iter *, struct keylist *,
			     struct disk_reservation *,
			     struct extent_insert_hook *, u64 *, unsigned);

int bch2_btree_insert(struct bch_fs *, enum btree_id, struct bkey_i *,
		     struct disk_reservation *,
		     struct extent_insert_hook *, u64 *, int flags);

int bch2_btree_delete_range(struct bch_fs *, enum btree_id,
			   struct bpos, struct bpos, struct bversion,
			   struct disk_reservation *,
			   struct extent_insert_hook *, u64 *);

int bch2_btree_node_rewrite(struct bch_fs *c, struct btree_iter *,
			    __le64, unsigned);
int bch2_btree_node_update_key(struct bch_fs *, struct btree_iter *,
			       struct btree *, struct bkey_i_extent *);

/* new transactional interface: */

void bch2_trans_update(struct btree_trans *, struct btree_iter *,
			     struct bkey_i *, unsigned);
int bch2_trans_commit(struct btree_trans *,
		      struct disk_reservation *,
		      struct extent_insert_hook *,
		      u64 *, unsigned);

#define bch2_trans_do(_c, _journal_seq, _flags, _do)			\
({									\
	struct btree_trans trans;					\
	int _ret;							\
									\
	bch2_trans_init(&trans, (_c));					\
									\
	do {								\
		bch2_trans_begin(&trans);				\
									\
		_ret = (_do) ?:	bch2_trans_commit(&trans, NULL, NULL,	\
					(_journal_seq), (_flags));	\
	} while (_ret == -EINTR);					\
									\
	bch2_trans_exit(&trans);					\
	_ret;								\
})

#endif /* _BCACHEFS_BTREE_UPDATE_H */
