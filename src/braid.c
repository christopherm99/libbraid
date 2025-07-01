#include <braid.h>
#include <braid/ctx.h>

#include <err.h>
#ifdef EBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

typedef struct cord Cord;

struct cord {
  ctx_t  ctx;
  void (*entry)(braid_t, usize);
  usize  val;
  uchar  flags;
  char  *name;
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
    uint  blocked;
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

static char *_strdup(const char *s) {
  char *d;
  if (s == NULL) return NULL;
  if ((d = malloc(strlen(s) + 1))) strcpy(d, s);
  return d;
}

braid_t braidinit(void) {
  Braid *b;

  if ((b = malloc(sizeof(Braid))) == NULL) err(EX_OSERR, "braidinit: malloc");
  memset(b, 0, sizeof(Braid));

  return b;
}

static void ripcord(usize b) {
  ((Braid *)b)->running->entry((Braid *)b, ((Braid *)b)->running->val);
  braidexit((Braid *)b);
}

void braidadd(braid_t b, void (*f)(braid_t, usize), usize stacksize, const char *name, uchar flags, usize arg) {
  Cord *c;

  if ((c = malloc(sizeof(Cord))) == NULL) err(EX_OSERR, "braidadd: malloc");
  memset(c, 0, sizeof(Cord));
  c->ctx = createctx(ripcord, stacksize, (usize)b);
  c->entry = f;
  c->name = _strdup(name);
  c->flags = flags;
  c->val = arg;

  braidappend(b, c);
}

void braidstart(braid_t _b) {
  Braid *b = (Braid *)_b;
  Cord *c;
  struct data *d;

  b->sched = newctx();

  for (;;) {
    if ((b->cords.count + b->cords.blocked) && (c = braidpop(b)) != NULL) {
      b->running = c;
#ifdef EBUG
    printf("braidstart: running cord %s\n", c->name ? c->name : "unamed");
#endif
      swapctx(b->sched, c->ctx);
    } else {
#ifdef EBUG
      printf("braidstart: done (%s)\n", c ? "system cords cancelled" : "all cords done");
#endif
      break;
    }
  }

  free(b->sched);
  while (c) {
    Cord *tmp = c;
    c = c->next;
    free(tmp->ctx);
    free(tmp->name);
    free(tmp);
  }

  d = b->data;
  while (d) {
    struct data *tmp = d;
    d = d->next;
    free(tmp);
  }
}

void braidyield(braid_t b) {
  if (((Braid *)b)->running == NULL) return;
  braidappend(b, ((Braid *)b)->running);
  swapctx(((Braid *)b)->running->ctx, ((Braid *)b)->sched);
}

usize braidblock(braid_t b) {
  ((Braid *)b)->cords.blocked++;
  swapctx(((Braid *)b)->running->ctx, ((Braid *)b)->sched);
  return ((Braid *)b)->running->val;
}

void braidunblock(braid_t b, cord_t c, usize val) {
  ((Braid *)b)->cords.blocked--;
  ((Cord *)c)->val = val;
  braidappend((Braid *)b, (Cord *)c);
#ifdef EBUG
  printf("braidunblock: unblocking cord %s\n", ((Cord *)c)->name ? ((Cord *)c)->name : "unamed");
#endif
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

