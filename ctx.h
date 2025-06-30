#ifndef _CTX_H
#define _CTX_H
#include <stdint.h>

typedef uintptr_t usize;
typedef unsigned char uchar;
typedef void* ctx_t;

ctx_t newctx(void);
ctx_t createctx(void (*f)(void), usize stacksize);
void swapctx(ctx_t old, ctx_t new);

#endif

