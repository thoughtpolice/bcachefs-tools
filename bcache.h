/*
 * Author: Kent Overstreet <kent.overstreet@gmail.com>
 *
 * GPLv2
 */

#ifndef _BCACHE_H
#define _BCACHE_H

#include "util.h"

extern const char * const cache_state[];
extern const char * const replacement_policies[];
extern const char * const csum_types[];
extern const char * const compression_types[];
extern const char * const error_actions[];
extern const char * const bdev_cache_mode[];
extern const char * const bdev_state[];

#endif /* _BCACHE_H */
