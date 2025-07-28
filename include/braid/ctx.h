#ifndef _BRAID_CTX_H
#define _BRAID_CTX_H

#include <braid/types.h>

extern ctx_t dummy_ctx;
ctx_t ctxempty(void);
ctx_t ctxcreate(void (*f)(usize), usize stacksize, usize arg);
void ctxdel(ctx_t c);
void ctxswap(ctx_t old, ctx_t new);
void *ctxstack(ctx_t c);

#endif

