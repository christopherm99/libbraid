#include "braid.h"

#include <err.h>
#include <stdlib.h>
#include <sysexits.h>

#ifdef __amd64__
typedef struct { usize rsp; usize regs[6]; } Context;
#elif defined __aarch64__
typedef struct { usize sp; usize x30; usize x19; usize regs[10]; double fpregs[8]; } Context;
#else
#error "Unsupported architecture"
#endif

typedef struct {
  Context ctx;
  uchar   stack[];
} Cord;

extern void ctx_swap(Context *, Context *);

static Cord start_cord;
static Cord *active_cord = &start_cord;
static void crash(void) { errx(EX_SOFTWARE, "a cord returned"); }

cord_t cordactive(void) { return active_cord; }

cord_t cordcreate(void (*f)(void), usize stacksize) {
  Cord *c;
  usize *p;

  if ((c = (Cord *)malloc(sizeof(Cord) + stacksize)) == NULL)
    err(EX_OSERR, "cordcreate: malloc");

#ifdef __amd64__
  p = (usize *)(((usize)c->stack + stacksize - 32) & ~0xF);
  *--p = (usize)crash;
  *--p = (usize)f;
  c->ctx.rsp = (usize)p;
#elif defined __aarch64__
  p = (usize *)(((usize)c->stack + stacksize - 32) & ~0xF);
  *--p = (usize)crash;
  *--p = (usize)crash;
  c->ctx.sp = (usize)p;
  c->ctx.x30 = (usize)f;
  c->ctx.x19 = (usize)p;
#endif
  return c;
}

void cordswitch(cord_t c) {
  register Cord *old = active_cord;
  ctx_swap(&old->ctx, &((active_cord = (Cord *)c))->ctx);
}

