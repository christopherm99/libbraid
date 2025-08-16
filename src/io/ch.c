#include <braid.h>
#include <braid/fd.h>
#include <braid/ch.h>

#include <errno.h>
#include <stdlib.h>

enum { SEND, RECV };

struct ch {
  char closed;
  struct elt {
    cord_t      c;
    usize       val;
    struct elt *next;
    uchar       type;
  } *head, *tail;
};

static inline struct elt *elt_peek(ch_t c) { return c->head; }
static inline struct elt *elt_pop(ch_t c) {
  struct elt *e;
  if ((e = c->head) && !(c->head = e->next)) c->tail = 0;
  return e;
}
static inline void elt_append(ch_t c, struct elt *e) {
  if (c->tail) c->tail->next = e;
  else c->head = e;
  c->tail = e;
}

ch_t chopen(braid_t b) { return braidzalloc(b, sizeof(struct ch)); }

void chclose(braid_t b, ch_t ch) {
  if (!ch->head) braidfree(b, ch);
  else {
    ch->closed = 1;
    if (ch->head->type == RECV) {
      for (struct elt *e = ch->head; e; e = e->next)
        braidunblock(b, e->c, 0);
      braidfree(b, ch);
    }
  }
}

int chsend(braid_t b, ch_t ch, usize data) {
  struct elt *e;
  if (ch->closed) {
    errno = EPIPE;
    return -1;
  }

retry:
  if ((e = elt_peek(ch)) && e->type == RECV) {
    elt_pop(ch);
    e->val = data;
    if (braidunblock(b, e->c, 1)) {
      braidfree(b, e);
      goto retry;
    }
    return 0;
  }

  if (!(e = braidzalloc(b, sizeof(struct elt)))) return -1;
  e->c = braidcurr(b);
  e->val = data;
  e->type = SEND;
  elt_append(ch, e);
  return braidblock(b);
}

#define r(ret,k) \
  do { \
    if (ok) *ok = k; \
    return ret; \
  } while (0)

usize chrecv(braid_t b, ch_t ch, char *ok) {
  usize ret;
  struct elt *e;
  if (ch->closed && !ch->head) r(0, 0);

  if ((e = elt_peek(ch)) && e->type == SEND) {
    ret = e->val;
    elt_pop(ch);
    braidunblock(b, e->c, 0);
    braidfree(b, e);
    r(ret, 1);
  }

  if (!(e = braidzalloc(b, sizeof(struct elt)))) r(0, 0);
  e->c = braidcurr(b);
  e->type = RECV;
  elt_append(ch, e);
  if (braidblock(b)) {
    ret = e->val;
    braidfree(b, e);
    r(ret, 1);
  } else {
    braidfree(b, e);
    r(0, 0);
  }
}

