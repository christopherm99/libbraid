#include <braid/ctx.h>

#include <err.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/mman.h>
#include <unistd.h>

#include "valgrind.h"

#define alloc(x) calloc(1, x)

#ifdef __amd64__
struct ctx {
  usize rsp, regs[6], rdi;
  void *stack;
  usize mapsize;
};
#elif defined __aarch64__
struct ctx {
  usize sp, x30, x19, regs[10];
  double fpregs[8];
  usize x0;
  void *stack;
  usize mapsize;
};
#else
#error "Unsupported architecture"
#endif

struct ctx dummy = {0};
ctx_t dummy_ctx = &dummy;

ctx_t ctxempty(void) {
  struct ctx *c;

  if ((c = (struct ctx *)alloc(sizeof(struct ctx))) == NULL) err(EX_OSERR, "ctxempty: alloc");

  return c;
}

ctx_t ctxcreate(void (*f)(usize), usize stacksize, usize arg) {
  ctx_t c;
  long pagesize = sysconf(_SC_PAGESIZE);
  size_t mapsize = ((stacksize + pagesize - 1) & ~(pagesize - 1)) + pagesize;
  usize *p;

  if (!(c = alloc(sizeof(struct ctx)))) err(EX_OSERR, "cordcreate: alloc");

  if ((c->stack = mmap(NULL, c->mapsize = mapsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    err(EX_OSERR, "ctxcreate: mmap");

  (void)VALGRIND_STACK_REGISTER(c->stack, c->stack + mapsize);

  /* TODO: don't assume the stack grows down */
  if (mprotect(c->stack, pagesize, PROT_NONE)) err(EX_OSERR, "ctxcreate: mprotect");

  p = (usize *)((usize)c->stack + mapsize);
#ifdef __amd64__
  *--p = (usize)f;
  c->rsp = (usize)p;
  c->rdi = (usize)arg;
#elif defined __aarch64__
  c->sp = (usize)p;
  c->x30 = (usize)f;
  c->x19 = (usize)p;
  c->x0 = (usize)arg;
#endif
  return c;
}

void ctxdel(ctx_t c) {
  void *tmp = c;
  if (c->stack && munmap(c->stack, c->mapsize)) err(EX_OSERR, "ctxdel: munmap");
  free(tmp);
}

void *ctxstack(ctx_t c) { return c->stack; }

