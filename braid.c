#include "braid.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "ctx.h"

#define stoa(s) ".RIDX"[s]

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

Braid *braidinit(void) {
  Braid *b;

  if ((b = malloc(sizeof(Braid))) == NULL) err(EX_OSERR, "braidinit: malloc");
  memset(b, 0, sizeof(Braid));

  return b;
}

static void ripcord(usize arg) {
  ((Braid *)arg)->running->entry();
  braidexit((Braid *)arg);
}

void braidadd(Braid *b, void (*f)(), usize stacksize, uint flags) {
  Cord *c;

  if ((c = malloc(sizeof(Cord))) == NULL) err(EX_OSERR, "braidadd: malloc");
  memset(c, 0, sizeof(Cord));
  c->ctx = createctx(ripcord, stacksize, (usize)b);
  c->entry = f;
  c->flags = flags;

  braidappend(b, c);
}

void braidlaunch(Braid *b) {
  Cord *c;

  b->sched = newctx();

  for (;;) {
    if ((c = braidpop(b)) == NULL || b->cords.count == 0) break;
    b->running = c;
    swapctx(b->sched, c->ctx);
  }

  free(b->sched);
  while (c) {
    Cord *tmp = c;
    c = c->next;
    free(tmp->ctx);
    free(tmp);
  }
}

void braidyield(Braid *b) {
  braidappend(b, b->running);
  swapctx(b->running->ctx, b->sched);
}

void braidexit(Braid *b) {
  free(b->running->ctx);
  free(b->running);
  swapctx(dummy_ctx, b->sched);
}

