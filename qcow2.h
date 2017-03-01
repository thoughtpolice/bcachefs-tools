#ifndef _QCOW2_H
#define _QCOW2_H

#include <linux/types.h>
#include "tools-util.h"

void qcow2_write_image(int, int, ranges *, unsigned);

#endif /* _QCOW2_H */
