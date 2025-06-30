#include "ctx.h"

#include <err.h>
#include <stdlib.h>
#include <sysexits.h>

#ifdef __amd64__
struct ctx { usize rsp; usize regs[6]; usize rdi; };
#elif defined __aarch64__
struct ctx { usize sp; usize x30; usize x19; usize regs[10]; double fpregs[8]; usize x0; };
#else
#error "Unsupported architecture"
#endif

static void crash(void) { errx(EX_SOFTWARE, "a cord returned"); }

ctx_t newctx(void) {
  struct ctx *c;

  if ((c = (struct ctx *)malloc(sizeof(struct ctx))) == NULL) err(EX_OSERR, "newctx: malloc");

  return c;
}

ctx_t createctx(void (*f)(usize), usize stacksize, usize arg) {
  struct ctx *c;
  usize *p;

  if ((c = (struct ctx *)malloc(sizeof(struct ctx) + stacksize)) == NULL)
    err(EX_OSERR, "cordcreate: malloc");

  p = (usize *)(((usize)c + sizeof(struct ctx) + stacksize - 32) & ~0xF);
#ifdef __amd64__
  *--p = (usize)crash;
  *--p = (usize)f;
  c->rsp = (usize)p;
  c->rdi = (usize)arg;
#elif defined __aarch64__
  *--p = (usize)crash;
  *--p = (usize)crash;
  c->sp = (usize)p;
  c->x30 = (usize)f;
  c->x19 = (usize)p;
  c->x0 = (usize)arg;
#endif
  return c;
}

