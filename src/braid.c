#include <braid.h>
#include <braid/ctx.h>

#include <err.h>
#ifdef EBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

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
    uint   blocked;
  } cords;
  struct data *data;
};

static cord_t braidpop(braid_t b) {
  cord_t c;

  if ((c = b->cords.head) == NULL) return NULL;
  b->cords.head = c->next;
  if (b->cords.head) b->cords.head->prev = NULL;
  else b->cords.tail = NULL;
  if (!(c->flags & CORD_SYSTEM)) b->cords.count--;

  return c;
}

static void braidappend(braid_t b, cord_t c) {
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
  braid_t b;

  if ((b = malloc(sizeof(struct braid))) == NULL) err(EX_OSERR, "braidinit: malloc");
  memset(b, 0, sizeof(struct braid));

  return b;
}

static void ripcord(usize b) {
  ((braid_t)b)->running->entry((braid_t)b, ((braid_t)b)->running->val);
  braidexit((braid_t)b);
}

void braidadd(braid_t b, void (*f)(braid_t, usize), usize stacksize, const char *name, uchar flags, usize arg) {
  cord_t c;

  if ((c = malloc(sizeof(struct cord))) == NULL) err(EX_OSERR, "braidadd: malloc");
  memset(c, 0, sizeof(struct cord));
  c->ctx = createctx(ripcord, stacksize, (usize)b);
  c->entry = f;
  c->name = _strdup(name);
  c->flags = flags;
  c->val = arg;

  braidappend(b, c);
}

void braidstart(braid_t b) {
  cord_t c;
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
  if (b->cords.count + b->cords.blocked) {
    while (c) {
      cord_t tmp = c;
      c = c->next;
      free(tmp->ctx);
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
  swapctx(b->running->ctx, b->sched);
}

usize braidblock(braid_t b) {
  b->cords.blocked++;
  swapctx(b->running->ctx, b->sched);
  return b->running->val;
}

void braidunblock(braid_t b, cord_t c, usize val) {
  b->cords.blocked--;
  c->val = val;
  braidappend(b, c);
#ifdef EBUG
  printf("braidunblock: unblocking cord %s\n", c->name ? c->name : "unamed");
#endif
}

void braidexit(braid_t b) {
  free(b->running->ctx);
  free(b->running);
  swapctx(dummy_ctx, b->sched);
}

cord_t braidcurr(braid_t b) { return b->running; }
uint braidcnt(braid_t b) { return b->cords.count; }

void **braiddata(braid_t b, uchar key) {
  struct data *d;

  for (d = b->data; d; d = d->next)
    if (d->key == key) return &d->data;

  if ((d = malloc(sizeof(struct data))) == NULL) err(EX_OSERR, "braiddata: malloc");
  memset(d, 0, sizeof(struct data));
  d->key = key;
  d->next = b->data;
  b->data = d;

  return &d->data;
}

