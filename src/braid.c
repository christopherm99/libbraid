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
#include "khashl.h"

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

#define hash_cord(c) kh_hash_uint64((uintptr_t)c)
KHASHL_SET_INIT(KH_LOCAL, cordset_t, cordset, void *, hash_cord, kh_eq_generic)

struct data {
  struct data *next;
  void        *data;
  uchar        key;
};

struct braid {
  ctx_t        start;
  cord_t       running;
  struct {
    cord_t head;
    cord_t tail;
  } cords;
  cordset_t   *blocked;
  cord_t       zombies;
  arena_t      arena;
  pool_t       lambda_pool;
  struct data *data;
};

static cord_t braidpop(braid_t b) {
  cord_t c;

  if ((c = b->cords.head) == NULL) return NULL;
  b->cords.head = c->next;
  if (b->cords.head) b->cords.head->prev = NULL;
  else b->cords.tail = NULL;

  return c;
}

static void braidappend(braid_t b, cord_t c) {
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
  if (write(2, buf, i) != i) abort();
}

void braidinfo(const braid_t b) {
  cord_t c;
  khint_t k;

  cordinfo(b->running, 'R');
  for (c = b->cords.head; c; c = c->next) cordinfo(c, 'r');
  kh_foreach(b->blocked, k) cordinfo(kh_key(b->blocked, k), 'b');
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
  b->blocked = cordset_init();
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
  cord_t ret;
  va_list args;
  va_start(args, nargs);
  ret = braidvadd(b, f, stacksize, name, flags, nargs, args);
  va_end(args);
  return ret;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
cord_t  braidvadd(braid_t b, void (*f)(), usize stacksize, const char *name, uchar flags, int nargs, va_list args) {
#pragma GCC diagnostic pop
  cord_t c;

  if ((c = alloc(sizeof(struct cord))) == NULL) err(EX_OSERR, "braidadd: alloc");
  for (int i = 0; i < 3; i++)
    if (!(*(void **)&c->lambdas[i] = pool_alloc(b->lambda_pool))) err(EX_OSERR, "braidadd: pool_alloc");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
  c->ctx = ctxcreate((void (*)(usize))
      lambda_compose(c->lambdas[0],
        lambda_bindldr(c->lambdas[1], (fn_t)braidexit, 0, 1, b),
        nargs ? lambda_vbindldr(c->lambdas[2], (fn_t)f, 0, nargs, args) : (fn_t)f),
      stacksize, (usize)b);
#pragma GCC diagnostic pop
  c->flags = flags;
  c->arena = arena_create();
  c->name = arena_strdup(c->arena, name);

  braidappend(b, c);
  return c;
}

static void schedule(braid_t b, ctx_t curr) {
  cord_t c;
  if ((braidcnt(b) || kh_size(b->blocked)) && (c = braidpop(b)) != NULL) {
    b->running = c;
    ctxswap(curr, c->ctx);
  } else ctxswap(curr, b->start);
  while ((c = b->zombies)) {
    b->zombies = c->next;
    corddel(b, c);
  }
}

static void segvhandler(int sig, siginfo_t *info, void *uap, braid_t b) {
  usize guardpage = (usize)ctxstack(b->running->ctx);
  if (((usize)info->si_addr >= guardpage) && ((usize)info->si_addr < guardpage + sysconf(_SC_PAGESIZE)))
    if (write(2, "stack overflow\n", 15) != 15) abort();
  braidinfo(b);
  abort();
  (void)uap, (void)sig;
}


void braidstart(braid_t b) {
  void *h1, *h2;
  stack_t ss, oss;
  struct sigaction sa = {0}, osa;
  cord_t c;

  b->start = ctxempty();

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
#pragma GCC diagnostic ignored "-Wcast-function-type"
  sa.sa_sigaction = (void (*)(int, siginfo_t *, void *))lambda_bindldr(h1, (fn_t)segvhandler, 3, 1, b);
  signal(SIGQUIT, (void (*)(int))lambda_bindldr(h2, (fn_t)braidinfo, 0, 1, b));
#pragma GCC diagnostic pop
  sigemptyset(&sa.sa_mask);
  foreachsig(sig, if (sigaction(sig, &sa, NULL)) err(EX_OSERR, "sigaction");)

  schedule(b, b->start);

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
  if (!sigaction(SIGQUIT, NULL, &osa) && ((usize)osa.sa_handler == (usize)h2)) signal(SIGQUIT, SIG_DFL);
  munmap(h1, LAMBDA_BIND_SIZE(0,1,0));

  while ((c = b->cords.head)) {
    b->cords.head = c->next;
    corddel(b, c);
  }

  {
    khint_t k;
    kh_foreach(b->blocked, k) corddel(b, kh_key(b->blocked, k));
    cordset_destroy(b->blocked);
  }

  ctxdel(b->start);
  arena_destroy(b->arena);
  free(b);
}

void braidyield(braid_t b) {
  if (b->running == NULL) return;
  braidappend(b, b->running);
  schedule(b, b->running->ctx);
}

usize braidblock(braid_t b) {
  int absent;
  cordset_put(b->blocked, b->running, &absent);
  schedule(b, b->running->ctx);
  return b->running->val;
}

int braidunblock(braid_t b, cord_t c, usize val) {
  khint_t k;
  if ((k = cordset_get(b->blocked, c)) < kh_end(b->blocked)) {
    cordset_del(b->blocked, k);
    c->val = val;
    braidappend(b, c);
    return 0;
  } else return -1;
}

void braidstop(braid_t b) {
  if (b->cords.tail) b->cords.tail->next = b->zombies;
  b->running->next = b->cords.head;
  b->zombies = b->running;
  b->cords.head = 0;
  schedule(b, dummy_ctx);
}

void braidexit(braid_t b) {
  b->running->next = b->zombies;
  b->zombies = b->running;
  schedule(b, dummy_ctx);
}

cord_t braidcurr(const braid_t b) { return b->running; }

uint braidcnt(const braid_t b) {
  uint ret = 0;
  for (cord_t c = b->cords.head; c; c = c->next)
    if (!(c->flags & CORD_SYSTEM)) ret++;
  return ret;
}

uint braidsys(const braid_t b) {
  uint ret = 0;
  for (cord_t c = b->cords.head; c; c = c->next)
    if (c->flags & CORD_SYSTEM) ret++;
  return ret;
}

uint braidblk(const braid_t b) { return kh_size(b->blocked); }

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
  khint_t k;
  if (c->prev) c->prev->next = c->next;
  if (c->next) c->next->prev = c->prev;
  if (b->cords.head == c) b->cords.head = c->next;
  if (b->cords.tail == c) b->cords.tail = c->prev;
  if ((k = cordset_get(b->blocked, c)) < kh_end(b->blocked)) cordset_del(b->blocked, k);
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

