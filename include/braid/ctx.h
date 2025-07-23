#ifndef _BRAID_CTX_H
#define _BRAID_CTX_H

#include <braid/types.h>

extern ctx_t dummy_ctx;
ctx_t newctx(void);
ctx_t createctx(void (*f)(usize), usize stacksize, usize arg);
void delctx(ctx_t c);
void swapctx(ctx_t old, ctx_t new);

#endif

