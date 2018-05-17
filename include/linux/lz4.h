#include <lz4.h>

#define LZ4_compress_destSize(src, dst, srclen, dstlen, workspace)	\
	LZ4_compress_destSize(src, dst, srclen, dstlen)
#define LZ4_MEM_COMPRESS 0
