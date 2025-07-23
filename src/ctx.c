#include <braid/ctx.h>

#include <err.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/mman.h>
#include <unistd.h>

#define alloc(x) calloc(1, x)

#ifdef __amd64__
struct ctx { usize rsp; usize regs[6]; usize rdi; void *stack; };
#elif defined __aarch64__
struct ctx { usize sp; usize x30; usize x19; usize regs[10]; double fpregs[8]; usize x0; void *stack; };
#else
#error "Unsupported architecture"
#endif

struct ctx dummy = {0};
ctx_t dummy_ctx = &dummy;

static void crash(void) { errx(EX_SOFTWARE, "bottom of stack"); }

ctx_t newctx(void) {
  struct ctx *c;

  if ((c = (struct ctx *)alloc(sizeof(struct ctx))) == NULL) err(EX_OSERR, "newctx: alloc");

  return c;
}

ctx_t createctx(void (*f)(usize), usize stacksize, usize arg) {
  ctx_t c;
  long pagesize = sysconf(_SC_PAGESIZE);
  size_t mapsize = (stacksize + pagesize) & ~(pagesize - 1);
  usize *p;

  if (!(c = alloc(sizeof(struct ctx)))) err(EX_OSERR, "cordcreate: alloc");

  if ((c->stack = mmap(NULL, mapsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    err(EX_OSERR, "createctx: mmap");
  c->sp = (usize)c->stack;

  if (mprotect(c->stack, pagesize, PROT_NONE)) err(EX_OSERR, "createctx: mprotect");

  p = (usize *)(c->sp + mapsize);
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

void delctx(ctx_t c) {
  if (c->stack && munmap(c->stack, sizeof(c->stack))) err(EX_OSERR, "delctx: munmap");
  free(c);
}

