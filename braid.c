#include "braid.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "ctx.h"

typedef struct cord Cord;

struct cord {
  ctx_t  ctx;
  void (*entry)(braid_t);
  usize  retval;
  uchar  flags;
  char   name[16];
  Cord  *next;
  Cord  *prev;
};

struct data {
  struct data *next;
  void        *data;
  uchar        key;
};

typedef struct {
  ctx_t sched;
  Cord *running;
  struct {
    Cord *head;
    Cord *tail;
    uint  count;
  } cords;
  struct data *data;
} Braid;

static Cord *braidpop(Braid *b) {
  Cord *c;

  if ((c = b->cords.head) == NULL) return NULL;
  b->cords.head = c->next;
  if (b->cords.head) b->cords.head->prev = NULL;
  else b->cords.tail = NULL;
  if (!(c->flags & CORD_SYSTEM)) b->cords.count--;

  return c;
}

static void braidappend(Braid *b, Cord *c) {
  if (!(c->flags & CORD_SYSTEM)) b->cords.count++;
  c->next = NULL;
  c->prev = b->cords.tail;
  if (b->cords.tail) b->cords.tail->next = c;
  else b->cords.head = c;
  b->cords.tail = c;
}

void braidaddcord(braid_t b, cord_t c, usize retval) {
  ((Braid *)b)->running->retval = retval;
  braidappend((Braid *)b, (Cord *)c);
}

braid_t braidinit(void) {
  Braid *b;

  if ((b = malloc(sizeof(Braid))) == NULL) err(EX_OSERR, "braidinit: malloc");
  memset(b, 0, sizeof(Braid));

  return b;
}

static void ripcord(usize arg) {
  ((Braid *)arg)->running->entry((Braid *)arg);
  braidexit((Braid *)arg);
}

void braidadd(braid_t b, void (*f)(braid_t), usize stacksize, uchar flags) {
  Cord *c;

  if ((c = malloc(sizeof(Cord))) == NULL) err(EX_OSERR, "braidadd: malloc");
  memset(c, 0, sizeof(Cord));
  c->ctx = createctx(ripcord, stacksize, (usize)b);
  c->entry = f;
  c->flags = flags;

  braidappend(b, c);
}

void braidstart(braid_t b) {
  Cord *c;
  struct data *d;

  ((Braid *)b)->sched = newctx();

  for (;;) {
    if ((c = braidpop(b)) == NULL || ((Braid *)b)->cords.count == 0) break;
    ((Braid *)b)->running = c;
    swapctx(((Braid *)b)->sched, c->ctx);
  }

  free(((Braid *)b)->sched);
  while (c) {
    Cord *tmp = c;
    c = c->next;
    free(tmp->ctx);
    free(tmp);
  }

  d = ((Braid *)b)->data;
  while (d) {
    struct data *tmp = d;
    d = d->next;
    free(tmp);
  }
}

void braidyield(braid_t b) {
  braidappend(b, ((Braid *)b)->running);
  swapctx(((Braid *)b)->running->ctx, ((Braid *)b)->sched);
}

usize braidblock(braid_t b) {
  swapctx(((Braid *)b)->running->ctx, ((Braid *)b)->sched);
  return ((Braid *)b)->running->retval;
}

void braidexit(braid_t b) {
  free(((Braid *)b)->running->ctx);
  free(((Braid *)b)->running);
  swapctx(dummy_ctx, ((Braid *)b)->sched);
}

cord_t braidcurr(braid_t b) { return ((Braid *)b)->running; }
uint braidcnt(braid_t *b) { return ((Braid *)b)->cords.count; }

void **braiddata(braid_t b, uchar key) {
  struct data *d;

  for (d = ((Braid *)b)->data; d; d = d->next)
    if (d->key == key) return &d->data;

  if ((d = malloc(sizeof(struct data))) == NULL) err(EX_OSERR, "braiddata: malloc");
  memset(d, 0, sizeof(struct data));
  d->key = key;
  d->next = ((Braid *)b)->data;
  ((Braid *)b)->data = d;

  return &d->data;
}

