#define _GNU_SOURCE
#include <braid.h>
#include <braid/ctx.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/mman.h>
#include <unistd.h>

#include "arena.h"
#include "lambda.h"
#include "pool.h"

#define alloc(x) calloc(1, x)

struct cord {
  cord_t  next, prev;
  ctx_t   ctx;
  usize   val;
  char   *name;
  arena_t arena;
  void   *lambdas[3];
  uchar   flags;
};

struct data {
  struct data *next;
  void        *data;
  uchar        key;
};

struct braid {
  ctx_t  sched;
  cord_t running;
  struct {
    cord_t head;
    cord_t tail;
    uint   count;
    uint   sys;
  } cords;
  struct {
    cord_t head;
    uint   count;
  } blocked;
  cord_t zombies;
  arena_t arena;
  pool_t lambda_pool;
  struct data *data;
};

static cord_t braidpop(braid_t b) {
  cord_t c;

  if ((c = b->cords.head) == NULL) return NULL;
  b->cords.head = c->next;
  if (b->cords.head) b->cords.head->prev = NULL;
  else b->cords.tail = NULL;
  if (c->flags & CORD_SYSTEM) b->cords.sys--;
  else b->cords.count--;

  return c;
}

static void braidappend(braid_t b, cord_t c) {
  if (c->flags & CORD_SYSTEM) b->cords.sys++;
  else b->cords.count++;
  c->next = NULL;
  c->prev = b->cords.tail;
  if (b->cords.tail) b->cords.tail->next = c;
  else b->cords.head = c;
  b->cords.tail = c;
}

static inline void corddel(braid_t b, cord_t c) {
  ctxdel(c->ctx);
  arena_destroy(c->arena);
  for (usize i = 0; i < sizeof(c->lambdas) / sizeof(*c->lambdas); i++)
    if (c->lambdas[i]) pool_free(b->lambda_pool, (void *)c->lambdas[i]);
  free(c);
}

static inline void cordinfo(const cord_t c, char state) {
  unsigned int i = 3;
  char buf[30] = {(c->flags & CORD_SYSTEM) ? 'S' : ' ', state, ' '};
  if (c->name) {
    for (; i < sizeof(buf) - 1 && c->name[i-3]; i++) buf[i] = c->name[i-3];
    if (i == sizeof(buf) - 1) memcpy(&buf[i-3], "...", 3);
  } else {
    memcpy(&buf[3], "<unamed>", 8);
    i += 8;
  }
  buf[i++] = '\n';
  write(2, buf, i);
}

void braidinfo(const braid_t b) {
  cord_t c;

  cordinfo(b->running, 'R');
  for (c = b->cords.head; c; c = c->next) cordinfo(c, 'r');
  for (c = b->blocked.head; c; c = c->next) cordinfo(c, 'b');
}

#ifdef __APPLE__
#define MMAP_PROT PROT_READ | PROT_WRITE
#define foreachsig(var,body) { int var = SIGSEGV; { body } var = SIGBUS; { body } }
#else
#define MMAP_PROT PROT_READ | PROT_WRITE | PROT_EXEC
#define foreachsig(var,body) { int var = SIGSEGV; { body } }
#endif

braid_t braidinit(void) {
  braid_t b;
  void *mem;

  if ((b = alloc(sizeof(struct braid))) == NULL) err(EX_OSERR, "braidinit: alloc");
  b->arena = arena_create();
  // seems like plenty of space...
  if ((mem = mmap(0, 1073741824, MMAP_PROT, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED) err(EX_OSERR, "mmap");
  b->lambda_pool = pool_new(mem, 1073741824, LAMBDA_BIND_SIZE(0,LAMBDA_BIND_MAX_ARGS-1,0));

  return b;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
cord_t braidadd(braid_t b, void (*f)(), usize stacksize, const char *name, uchar flags, int nargs, ...) {
#pragma GCC diagnostic pop
  cord_t c;
  va_list args;

  if ((c = alloc(sizeof(struct cord))) == NULL) err(EX_OSERR, "braidadd: alloc");
  va_start(args, nargs);
  for (int i = 0; i < 3; i++)
    if (!(*(void **)&c->lambdas[i] = pool_alloc(b->lambda_pool))) err(EX_OSERR, "braidadd: pool_alloc");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-mismatch"
  c->ctx = ctxcreate((void (*)(usize))
      lambda_compose(c->lambdas[0],
        lambda_bindldr(c->lambdas[1], (fn_t)braidexit, 0, 1, b),
        lambda_vbindldr(c->lambdas[2], (fn_t)f, 1, nargs, args)),
      stacksize, (usize)b);
#pragma GCC diagnostic pop
  va_end(args);
  c->flags = flags;
  c->arena = arena_create();
  c->name = arena_strdup(c->arena, name);

  braidappend(b, c);
  return c;
}

static void segvhandler(int sig, siginfo_t *info, void *uap, braid_t b) {
  usize guardpage = (usize)ctxstack(b->running->ctx);
  if (((usize)info->si_addr >= guardpage) && ((usize)info->si_addr < guardpage + sysconf(_SC_PAGESIZE)))
    write(2, "stack overflow\n", 15);
  braidinfo(b);
  abort();
  (void)uap, (void)sig;
}


void braidstart(braid_t b) {
  void *h1, *h2;
  stack_t ss, oss;
  struct sigaction sa = {0}, osa;
  cord_t c;

  b->sched = ctxempty();

  /* install signal handlers */
  if ((h1 = mmap(NULL, LAMBDA_BIND_SIZE(0,1,0) + LAMBDA_BIND_SIZE(0,1,0), MMAP_PROT, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED)
    err(EX_OSERR, "mmap");
  h2 = ((char *)h1 + LAMBDA_BIND_SIZE(0,1,0));
  if (!(ss.ss_sp = braidmalloc(b, SIGSTKSZ))) err(EX_OSERR, "alloc sigstack");
  ss.ss_size = SIGSTKSZ;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL)) err(EX_OSERR, "sigaltstack");
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-mismatch"
  sa.sa_sigaction = (void (*)(int, struct __siginfo *, void *))lambda_bindldr(h1, (fn_t)segvhandler, 3, 1, b);
  signal(SIGQUIT, (void (*)(int))lambda_bindldr(h2, (fn_t)braidinfo, 0, 1, b));
#pragma GCC diagnostic pop
  sigemptyset(&sa.sa_mask);
  foreachsig(sig, if (sigaction(sig, &sa, NULL)) err(EX_OSERR, "sigaction");)

  for (;;) {
    while ((c = b->zombies)) {
      b->zombies = c->next;
      corddel(b, c);
    }
    if ((b->cords.count + b->blocked.count) && (c = braidpop(b)) != NULL) {
      b->running = c;
      ctxswap(b->sched, c->ctx);
    } else {
      break;
    }
  }

  /* uninstall signal handlers */
  if (!sigaltstack(NULL, &oss) && oss.ss_sp == ss.ss_sp) {
    stack_t nss = {0};
    nss.ss_flags = SS_DISABLE;
    sigaltstack(&nss, NULL);
  }
  foreachsig(
    sig,
    if (!sigaction(sig, NULL, &osa) && (osa.sa_sigaction == sa.sa_sigaction)) {
      struct sigaction nsa = {0};
      nsa.sa_handler = SIG_DFL;
      sigemptyset(&nsa.sa_mask);
      sigaction(sig, &nsa, NULL);
    }
  )
  if (!sigaction(SIGQUIT, NULL, &osa) && ((void *)osa.sa_handler == h2)) signal(SIGQUIT, SIG_DFL);
  munmap(h1, LAMBDA_BIND_SIZE(0,1,0));

  while ((c = b->cords.head)) {
    b->cords.head = c->next;
    corddel(b, c);
  }

  while ((c = b->blocked.head)) {
    b->blocked.head = c->next;
    corddel(b, c);
  }

  arena_destroy(b->arena);
  ctxdel(b->sched);
  free(b);
}

void braidyield(braid_t b) {
  if (b->running == NULL) return;
  braidappend(b, b->running);
  ctxswap(b->running->ctx, b->sched);
}

usize braidblock(braid_t b) {
  b->blocked.count++;
  if (b->blocked.head) b->blocked.head->prev = b->running;
  b->running->next = b->blocked.head;
  b->running->prev = NULL;
  b->blocked.head = b->running;
  ctxswap(b->running->ctx, b->sched);
  return b->running->val;
}

int braidunblock(braid_t b, cord_t c, usize val) {
  cord_t curr;
  b->blocked.count--;
  for (curr = b->blocked.head; curr; curr = curr->next)
    if (curr == c) {
      if (curr->prev) {
        curr->prev->next = curr->next;
        if (curr->next) curr->next->prev = curr->prev;
      } else {
        b->blocked.head = curr->next;
        if (curr->next) curr->next->prev = NULL;
      }
      goto found;
    }
  return -1;
found:
  c->val = val;
  braidappend(b, c);
  return 0;
}

void braidstop(braid_t b) {
  if (b->cords.tail) b->cords.tail->next = b->zombies;
  b->running->next = b->cords.head;
  b->zombies = b->running;
  b->cords.head = 0;
  ctxswap(dummy_ctx, b->sched);
}

void braidexit(braid_t b) {
  b->running->next = b->zombies;
  b->zombies = b->running;
  ctxswap(dummy_ctx, b->sched);
}

cord_t braidcurr(const braid_t b) { return b->running; }
uint braidcnt(const braid_t b) { return b->cords.count; }
uint braidsys(const braid_t b) { return b->cords.sys; }

void **braiddata(braid_t b, uchar key) {
  struct data *d;

  for (d = b->data; d; d = d->next)
    if (d->key == key) return &d->data;

  if ((d = braidzalloc(b, sizeof(struct data))) == NULL) err(EX_OSERR, "braiddata: alloc");
  d->key = key;
  d->next = b->data;
  b->data = d;

  return &d->data;
}

void cordhalt(braid_t b, cord_t c) {
  if (c->prev) c->prev->next = c->next;
  if (c->next) c->next->prev = c->prev;
  if (b->cords.head == c) b->cords.head = c->next;
  if (b->cords.tail == c) b->cords.tail = c->prev;
  if (b->blocked.head == c) b->blocked.head = c->next;
  c->next = b->zombies;
  b->zombies = c;
}

void *cordmalloc(cord_t c, size_t sz) { return arena_malloc(c->arena, sz); }
void *cordzalloc(cord_t c, size_t sz) { return arena_zalloc(c->arena, sz); }
void *cordrealloc(cord_t c, void *p, size_t sz) { return arena_realloc(c->arena, p, sz); }
void  cordfree(cord_t c, void *p) { arena_free(c->arena, p); }
void *braidmalloc(braid_t b, size_t sz) { return arena_malloc(b->arena, sz); }
void *braidzalloc(braid_t b, size_t sz) { return arena_zalloc(b->arena, sz); }
void *braidrealloc(braid_t b, void *p, size_t sz) { return arena_realloc(b->arena, p, sz); }
void  braidfree(braid_t b, void *p) { arena_free(b->arena, p); }

