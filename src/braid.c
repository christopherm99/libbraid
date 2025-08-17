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
#include "uthash.h"

#define alloc(x) calloc(1, x)

typedef struct {
  ctx_t   ctx;
  usize   val;
  char   *name;
  arena_t arena;
  void   *lambdas[3];
  uchar   flags;
  cord_t  id;
  UT_hash_handle hh;
} cord_s;

typedef cord_s * cordset_t;

#define HASH_FIND_CORD(head,findcord,out) HASH_FIND(hh,head,findcord,sizeof(cord_t),out)
#define HASH_ADD_CORD(head,add) HASH_ADD(hh,head,id,sizeof(cord_t),add)

struct data {
  struct data *next;
  void        *data;
  uchar        key;
};

struct braid {
  ctx_t        start;
  cord_s      *curr;
  cordset_t    runable;
  cordset_t    blocked;
  cordset_t    zombies;
  arena_t      arena;
  pool_t       lambda_pool;
  struct data *data;
  uint64_t     count;
};

static cord_s *braidcurrs(const braid_t b) { return b->curr; }
static cord_s *getcord(cordset_t cs, cord_t c) {
  cord_s *ret;
  HASH_FIND_CORD(cs, &c, ret);
  return ret;
}

static cord_s *braidpop(braid_t b) {
  cord_s *c;

  if (!b->runable) return NULL;
  c = b->runable;
  HASH_DEL(b->runable, b->runable);

  return c;
}

static void braidappend(braid_t b, cord_s *c) { HASH_ADD_CORD(b->runable, c); }

static inline void corddel(braid_t b, cord_s *c) {
  ctxdel(c->ctx);
  arena_destroy(c->arena);
  for (usize i = 0; i < sizeof(c->lambdas) / sizeof(*c->lambdas); i++)
    if (c->lambdas[i]) pool_free(b->lambda_pool, (void *)c->lambdas[i]);
  free(c);
}

static inline void cordinfo(const cord_s *c, char state) {
  uint i = 3;
  char buf[36] = {(c->flags & CORD_SYSTEM) ? 'S' : ' ', state, ' '};
  buf[i++] = c->id > 9999 ? '+' : ' ';
  for (uint j = 1000; j > 1; j /= 10) buf[i++] = c->id / j ? '0' + (c->id / j % 10) : ' ';
  buf[i++] = '0' + c->id % 10;
  buf[i++] = ' ';
  if (c->name) {
    for (uint j = 0; i < sizeof(buf) - 1 && c->name[j]; i++, j++) buf[i] = c->name[j];
    if (i == sizeof(buf) - 1) memcpy(&buf[i-3], "...", 3);
  } else {
    memcpy(&buf[3], "<unamed>", 8);
    i += 8;
  }
  buf[i++] = '\n';
  if (write(2, buf, i) != i) abort();
}

void braidinfo(const braid_t b) {
  cordinfo(b->curr, 'R');
  for (cord_s *c = b->runable; c; c = c->hh.next) cordinfo(c, 'r');
  for (cord_s *c = b->blocked; c; c = c->hh.next) cordinfo(c, 'b');
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
  cord_s *c;

  if ((c = alloc(sizeof(cord_s))) == NULL) err(EX_OSERR, "braidadd: alloc");
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

  while (getcord(b->runable, b->count)) b->count++;
  c->id = b->count++;

  braidappend(b, c);
  return c->id;
}

static void schedule(braid_t b, ctx_t curr) {
  cord_s *c, *tmp;
  if ((braidcnt(b) || braidblk(b)) && (c = braidpop(b)) != NULL) {
    b->curr = c;
    ctxswap(curr, c->ctx);
  } else ctxswap(curr, b->start);
  HASH_ITER(hh, b->zombies, c, tmp) {
    HASH_DEL(b->zombies, c);
    corddel(b, c);
  }
}

static void segvhandler(int sig, siginfo_t *info, void *uap, braid_t b) {
  usize guardpage = (usize)ctxstack(b->curr->ctx);
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

  b->start = ctxempty();

  /* install signal handlers */
  if ((h1 = mmap(NULL, LAMBDA_BIND_SIZE(0,1,0) * 2, MMAP_PROT, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED)
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
  munmap(h1, LAMBDA_BIND_SIZE(0,1,0) * 2);

  {
    cord_s *c, *tmp;
    HASH_ITER(hh, b->runable, c, tmp) {
      HASH_DEL(b->runable, c);
      corddel(b, c);
    }

    HASH_ITER(hh, b->blocked, c, tmp) {
      HASH_DEL(b->blocked, c);
      corddel(b, c);
    }
  }

  ctxdel(b->start);
  arena_destroy(b->arena);
  free(b);
}

void braidyield(braid_t b) {
  if (b->curr == NULL) return;
  braidappend(b, b->curr);
  schedule(b, b->curr->ctx);
}

usize braidblock(braid_t b) {
  HASH_ADD_CORD(b->blocked, braidcurrs(b));
  schedule(b, b->curr->ctx);
  return b->curr->val;
}

int braidunblock(braid_t b, cord_t c, usize val) {
  cord_s *tgt;
  HASH_FIND_CORD(b->blocked, &c, tgt);
  if (tgt) {
    HASH_DEL(b->blocked, tgt);
    tgt->val = val;
    braidappend(b, tgt);
    return 0;
  } else return -1;
}

void braidstop(braid_t b) {
  cord_s *c, *tmp;
  HASH_ITER(hh, b->runable, c, tmp) {
    HASH_DEL(b->runable, c);
    corddel(b, c);
  }
  HASH_ADD_CORD(b->zombies, braidcurrs(b));
  schedule(b, dummy_ctx);
}

void braidexit(braid_t b) {
  HASH_ADD_CORD(b->zombies, braidcurrs(b));
  schedule(b, dummy_ctx);
}

cord_t braidcurr(const braid_t b) { return b->curr->id; }

uint braidcnt(const braid_t b) {
  uint ret = 0;
  for (cord_s *c = b->runable; c; c = c->hh.next)
    if (!(c->flags & CORD_SYSTEM)) ret++;
  return ret;
}

uint braidsys(const braid_t b) {
  uint ret = 0;
  for (cord_s *c = b->runable; c; c = c->hh.next)
    if (c->flags & CORD_SYSTEM) ret++;
  return ret;
}

uint braidblk(const braid_t b) { return HASH_COUNT(b->blocked); }

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
  cord_s *tgt;
  HASH_FIND_CORD(b->runable, &c, tgt);
  if (tgt) {
    HASH_DEL(b->runable, tgt);
    HASH_ADD_CORD(b->zombies, tgt);
  } else {
    HASH_FIND_CORD(b->blocked, &c, tgt);
    if (tgt) {
      HASH_DEL(b->blocked, tgt);
      HASH_ADD_CORD(b->zombies, tgt);
    }
  }
}

void *cordmalloc(braid_t b, cord_t c, size_t sz) {
  cord_s *cs = getcord(b->runable, c);
  if (!cs) cs = getcord(b->blocked, c);
  if (!cs) return 0;
  return arena_malloc(cs->arena, sz);
}

void *cordzalloc(braid_t b, cord_t c, size_t sz) {
  cord_s *cs = getcord(b->runable, c);
  if (!cs) cs = getcord(b->blocked, c);
  if (!cs) return 0;
  return arena_zalloc(cs->arena, sz);
}

void *cordrealloc(braid_t b, cord_t c, void *p, size_t sz) {
  cord_s *cs = getcord(b->runable, c);
  if (!cs) cs = getcord(b->blocked, c);
  if (!cs) return 0;
  return arena_realloc(cs->arena, p, sz);
}

void  cordfree(braid_t b, cord_t c, void *p) {
  cord_s *cs = getcord(b->runable, c);
  if (!cs) cs = getcord(b->blocked, c);
  if (!cs) return;
  arena_free(cs->arena, p);
}

void *braidmalloc(braid_t b, size_t sz) { return arena_malloc(b->arena, sz); }
void *braidzalloc(braid_t b, size_t sz) { return arena_zalloc(b->arena, sz); }
void *braidrealloc(braid_t b, void *p, size_t sz) { return arena_realloc(b->arena, p, sz); }
void  braidfree(braid_t b, void *p) { arena_free(b->arena, p); }

