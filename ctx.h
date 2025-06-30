#ifndef _CTX_H
#define _CTX_H

#include "types.h"

extern ctx_t dummy_ctx;
ctx_t newctx(void);
ctx_t createctx(void (*f)(usize), usize stacksize, usize arg);
void swapctx(ctx_t old, ctx_t new);

#endif

