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

#include "lambda.h"

#define alloc(x) calloc(1, x)

struct cord {
  ctx_t  ctx;
  void (*entry)(braid_t, usize);
  usize  val;
  uchar  flags;
  char  *name;
  cord_t next;
  cord_t prev;
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

static char *_strdup(const char *s) {
  char *d;
  if (s == NULL) return NULL;
  if ((d = malloc(strlen(s) + 1))) strcpy(d, s);
  return d;
}

static inline void cordinfo(cord_t c, char state) {
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

void braidinfo(braid_t b) {
  cord_t c;

  cordinfo(b->running, 'R');
  for (c = b->cords.head; c; c = c->next) cordinfo(c, 'r');
  for (c = b->blocked.head; c; c = c->next) cordinfo(c, 'b');
}

braid_t braidinit(void) {
  braid_t b;

  if ((b = alloc(sizeof(struct braid))) == NULL) err(EX_OSERR, "braidinit: alloc");

  return b;
}

static void ripcord(usize _b) {
  braid_t b = (braid_t)_b;
  b->running->entry(b, b->running->val);
  braidexit((braid_t)b);
}

cord_t braidadd(braid_t b, void (*f)(braid_t, usize), usize stacksize, const char *name, uchar flags, usize arg) {
  cord_t c;

  if ((c = alloc(sizeof(struct cord))) == NULL) err(EX_OSERR, "braidadd: alloc");
  c->ctx = ctxcreate(ripcord, stacksize, (usize)b);
  c->entry = f;
  c->name = _strdup(name);
  c->flags = flags;
  c->val = arg;

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

#ifdef __APPLE__
#define MMAP_PROT PROT_READ | PROT_WRITE
#define foreachsig(var,body) { int var = SIGSEGV; { body } var = SIGBUS; { body } }
#else
#define MMAP_PROT PROT_READ | PROT_WRITE | PROT_EXEC
#define foreachsig(var,body) { int var = SIGSEGV; { body } }
#endif

void braidstart(braid_t b) {
  void *mem;
  void (*h1)(), (*h2)();
  stack_t ss, oss;
  struct sigaction sa = {0}, osa;
  cord_t c;
  struct data *d;

  b->sched = ctxempty();

  /* install signal handlers */
  if ((mem = mmap(NULL, LAMBDA_SIZE(0,1,0) + LAMBDA_SIZE(0,1,0), MMAP_PROT, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED)
    err(EX_OSERR, "mmap");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
  h1 = (usize (*)())mem;
  h2 = (usize (*)())((char *)mem + LAMBDA_SIZE(0,1,0));
#pragma GCC diagnostic pop
  if (!(ss.ss_sp = malloc(SIGSTKSZ))) err(EX_OSERR, "mmap");
  ss.ss_size = SIGSTKSZ;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL)) err(EX_OSERR, "sigaltstack");
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
  sa.sa_sigaction = lambda_bind(h1, segvhandler, 4, MOV(0), MOV(1), MOV(2), LDR((usize)b));
  sigemptyset(&sa.sa_mask);
  foreachsig(sig, if (sigaction(sig, &sa, NULL)) err(EX_OSERR, "sigaction");)
  signal(SIGQUIT, lambda_bind(h2, braidinfo, 1, LDR((usize)b)));

  for (;;) {
    if ((b->cords.count + b->blocked.count) && (c = braidpop(b)) != NULL) {
      b->running = c;
#ifdef EBUG
    printf("braidstart: running cord %s\n", c->name ? c->name : "unamed");
#endif
      ctxswap(b->sched, c->ctx);
    } else {
#ifdef EBUG
      printf("braidstart: done (%s)\n", c ? "system cords cancelled" : "all cords done");
#endif
      break;
    }
  }

  /* uninstall signal handlers */
  if (!sigaltstack(NULL, &oss) && oss.ss_sp == ss.ss_sp) {
    stack_t nss = {0};
    nss.ss_flags = SS_DISABLE;
    sigaltstack(&nss, NULL);
  }
  free(ss.ss_sp);
  foreachsig(
    sig,
    if (!sigaction(sig, NULL, &osa) && (osa.sa_sigaction == sa.sa_sigaction)) {
      struct sigaction nsa = {0};
      nsa.sa_handler = SIG_DFL;
      sigemptyset(&nsa.sa_mask);
      sigaction(sig, &nsa, NULL);
    }
  )
  if (!sigaction(SIGQUIT, NULL, &osa) && (osa.sa_handler == h2)) signal(SIGQUIT, SIG_DFL);
  munmap(mem, LAMBDA_SIZE(0,1,0));

  free(b->sched);
  if (b->cords.count + b->blocked.count) {
    while (c) {
      cord_t tmp = c;
      c = c->next;
      ctxdel(tmp->ctx);
      free(tmp->name);
      free(tmp);
    }
  }

  d = b->data;
  while (d) {
    struct data *tmp = d;
    d = d->next;
    free(tmp);
  }
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
#ifdef EBUG
  printf("braidblock: blocking cord %s\n", b->running->name ? b->running->name : "unamed");
#endif
  ctxswap(b->running->ctx, b->sched);
  return b->running->val;
}

void braidunblock(braid_t b, cord_t c, usize val) {
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
  errx(EX_SOFTWARE, "braidunblock: cord (%s) not found in blocked list", c->name ? c->name : "unamed");
found:
  c->val = val;
  braidappend(b, c);
#ifdef EBUG
  printf("braidunblock: unblocking cord %s\n", c->name ? c->name : "unamed");
#endif
}

void braidexit(braid_t b) {
  ctxdel(b->running->ctx);
  free(b->running->name);
  free(b->running);
  ctxswap(dummy_ctx, b->sched);
}

cord_t braidcurr(braid_t b) { return b->running; }
uint braidcnt(braid_t b) { return b->cords.count; }
uint braidsys(braid_t b) { return b->cords.sys; }

void **braiddata(braid_t b, uchar key) {
  struct data *d;

  for (d = b->data; d; d = d->next)
    if (d->key == key) return &d->data;

  if ((d = alloc(sizeof(struct data))) == NULL) err(EX_OSERR, "braiddata: alloc");
  d->key = key;
  d->next = b->data;
  b->data = d;

  return &d->data;
}

usize *cordarg(cord_t c) { return &c->val; }

void cordhalt(braid_t b, cord_t c) {
  if (c->prev) c->prev->next = c->next;
  if (c->next) c->next->prev = c->prev;
  if (b->cords.head == c) b->cords.head = c->next;
  if (b->cords.tail == c) b->cords.tail = c->prev;
  if (b->blocked.head == c) b->blocked.head = c->next;
  ctxdel(c->ctx);
  free(c->name);
  free(c);
}

