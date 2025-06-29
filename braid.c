#include "braid.h"

#include <err.h>
#include <stdlib.h>
#include <sysexits.h>

#ifdef __amd64__
typedef struct {
  usize rsp;
  usize rbp;
  usize rbx;
  usize r12;
  usize r13;
  usize r14;
  usize r15;
} Context;
#elif defined __aarch64__
typedef struct {
  usize sp;
  usize x30;
  usize x19;
  usize x20;
  usize x21;
  usize x22;
  usize x23;
  usize x24;
  usize x25;
  usize x26;
  usize x27;
  usize x28;
  usize x29;
  double d8;
  double d9;
  double d10;
  double d11;
  double d12;
  double d13;
  double d14;
  double d15;
} Context;
#endif

typedef struct {
  Context ctx;
  uchar   stack[];
} Cord;

extern void ctx_swap(Context *, Context *);

static Cord active_cord;
static void crash(void) { err(EX_SOFTWARE, "a cord returned"); }

cord_t cordactive(void) {
  return &active_cord;
}

cord_t cordcreate(void (*f)(void), usize stacksize) {
  Cord *c;
  usize *p;

  if ((c = (Cord *)malloc(sizeof(Cord) + stacksize)) == NULL)
    err(EX_OSERR, "cordcreate: malloc");

#ifdef __amd64__
  p = (usize *)(((usize)c->stack + stacksize - 32) & ~0xF);
  *--p = (usize)crash;
  *--p = (usize)f;
  c->ctx.rsp = (usize)(c->stack + stacksize);
#elif defined __aarch64__
  p = (usize *)(((usize)c->stack + stacksize - 32) & ~0xF);
  *--p = (usize)crash;
  *--p = (usize)crash;
  c->ctx.sp = (usize)p;
  c->ctx.x30 = (usize)f;
  c->ctx.x19 = (usize)p;
#else
#error "Unsupported architecture"
#endif
  return c;
}

void cordswitch(cord_t c) {
  register Cord *old = &active_cord;
  ctx_swap(&old->ctx, &((Cord *)c)->ctx);
}

