#include <zstd.h>

#define ZSTD_initDCtx(w, s)	ZSTD_initStaticDCtx(w, s)
#define ZSTD_initCCtx(w, s)	ZSTD_initStaticCCtx(w, s)

#define ZSTD_compressCCtx(w, dst, d_len, src, src_len, params)	\
	ZSTD_compressCCtx(w, dst, d_len, src, src_len, 0)

#define ZSTD_CCtxWorkspaceBound(p)	ZSTD_estimateCCtxSize(0)
#define ZSTD_DCtxWorkspaceBound()	ZSTD_estimateDCtxSize()
