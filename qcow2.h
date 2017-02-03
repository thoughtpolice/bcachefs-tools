#ifndef _QCOW2_H
#define _QCOW2_H

#include <linux/types.h>
#include "ccan/darray/darray.h"

struct range {
	u64		start;
	u64		end;
};

typedef darray(struct range) sparse_data;

static inline void data_add(sparse_data *data, u64 offset, u64 size)
{
	darray_append(*data, (struct range) {
		.start = offset,
		.end = offset + size
	});
}

void qcow2_write_image(int, int, sparse_data *, unsigned);

#endif /* _QCOW2_H */
