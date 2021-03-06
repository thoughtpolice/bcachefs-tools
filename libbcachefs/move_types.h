#ifndef _BCACHEFS_MOVE_TYPES_H
#define _BCACHEFS_MOVE_TYPES_H

struct bch_move_stats {
	enum bch_data_type	data_type;
	struct btree_iter	iter;

	atomic64_t		keys_moved;
	atomic64_t		sectors_moved;
	atomic64_t		sectors_seen;
	atomic64_t		sectors_raced;
};

#endif /* _BCACHEFS_MOVE_TYPES_H */
