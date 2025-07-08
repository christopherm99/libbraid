#include <braid.h>
#include <braid/ch.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#define alloc(x) calloc(1, x)

struct chctx {
  struct {
    struct {
      struct sendelt {
        cord_t          cord;
        usize           data;
        struct sendelt *next;
      } *head, *tail;
    } send;
    struct {
      struct recvelt {
        cord_t          cord;
        struct recvelt *next;
      } *head, *tail;
    } recv;
    uchar reserved;
  } *chs;
  uint max, cap;
};

static struct chctx *chctx(braid_t b) {
  struct chctx **ctx = (struct chctx **)braiddata(b, BRAID_CH_KEY);
  if (*ctx) return *ctx;
  if ((*ctx = alloc(sizeof(struct chctx))) == NULL) err(EX_OSERR, "ensure_chctx: alloc");
  return *ctx;
}

static struct chctx *wait_ctx(braid_t b) {
  struct chctx **ctx = (struct chctx **)braiddata(b, BRAID_CH_KEY);
  while (!*ctx) braidyield(b);
  return *ctx;
}

static ch_t ch_append(struct chctx *ctx) {
  if (ctx->max + 1 > ctx->cap) {
    ctx->cap = (ctx->cap == 0) ? 4 : ctx->cap * 2;
    if ((ctx->chs = realloc(ctx->chs, ctx->cap * sizeof(*ctx->chs))) == NULL)
      err(EX_OSERR, "ch_append: realloc");
  }
  memset(&ctx->chs[ctx->max], 0, sizeof(ctx->chs[0]));
  ctx->chs[ctx->max].reserved = 1;
  return ctx->max++;
}

void chvisor(braid_t b, usize _) {
  struct chctx *ctx = chctx(b);
  (void)_;

  for (;;) {
    if (ctx->max) {
      uint i;
      for (i = 0; i < ctx->max; i++) {
        while (ctx->chs[i].send.head && ctx->chs[i].recv.head) {
          cord_t send = ctx->chs[i].send.head->cord;
          cord_t recv = ctx->chs[i].recv.head->cord;
          usize data = ctx->chs[i].send.head->data;

          if ((ctx->chs[i].send.head = ctx->chs[i].send.head->next) == NULL)
            ctx->chs[i].send.tail = NULL;
          if ((ctx->chs[i].recv.head = ctx->chs[i].recv.head->next) == NULL)
            ctx->chs[i].recv.tail = NULL;

          braidunblock(b, send, 0);
          braidunblock(b, recv, data);
        }
      }
    }
    braidyield(b);
  }
}

ch_t chcreate(braid_t b) {
  struct chctx *ctx = chctx(b);
  uint i;

  for (i = 0; i < ctx->max; i++) {
    if (!ctx->chs[i].reserved) {
      ctx->chs[i].reserved = 1;
      return i;
    }
  }

  return ch_append(ctx);
}

void chdestroy(braid_t b, ch_t ch) {
  struct chctx *ctx = chctx(b);

  if (ch >= ctx->max || !ctx->chs[ch].reserved) {
    warnx("chdestroy: invalid channel %u", ch);
    return;
  }

  while (ctx->chs[ch].send.head) {
    struct sendelt *tmp = ctx->chs[ch].send.head;
    ctx->chs[ch].send.head = tmp->next;
    braidunblock(b, tmp->cord, 1);
    free(tmp);
  }

  while (ctx->chs[ch].recv.head) {
    struct recvelt *tmp = ctx->chs[ch].recv.head;
    ctx->chs[ch].recv.head = tmp->next;
    braidunblock(b, tmp->cord, 0);
    free(tmp);
  }

  memset(&ctx->chs[ch], 0, sizeof(ctx->chs[0]));

  if (ch == ctx->max - 1) ctx->max--;
}

uint chsend(braid_t b, ch_t ch, usize data) {
  struct chctx *ctx = wait_ctx(b);

  if (ctx->chs[ch].send.tail) {
    if ((ctx->chs[ch].send.tail->next = alloc(sizeof(struct sendelt))) == NULL)
      err(EX_OSERR, "chsend: alloc");
    ctx->chs[ch].send.tail = ctx->chs[ch].send.tail->next;
  } else {
    if ((ctx->chs[ch].send.head = alloc(sizeof(struct sendelt))) == NULL)
      err(EX_OSERR, "chsend: alloc");
    ctx->chs[ch].send.tail = ctx->chs[ch].send.head;
  }
  ctx->chs[ch].send.tail->cord = braidcurr(b);
  ctx->chs[ch].send.tail->data = data;

  return (int)braidblock(b);
}

usize chrecv(braid_t b, ch_t ch) {
  struct chctx *ctx = wait_ctx(b);

  if (ctx->chs[ch].recv.tail) {
    if ((ctx->chs[ch].recv.tail->next = alloc(sizeof(struct recvelt))) == NULL)
      err(EX_OSERR, "chrecv: alloc");
    ctx->chs[ch].recv.tail = ctx->chs[ch].recv.tail->next;
  } else {
    if ((ctx->chs[ch].recv.head = alloc(sizeof(struct recvelt))) == NULL)
      err(EX_OSERR, "chrecv: alloc");
    ctx->chs[ch].recv.tail = ctx->chs[ch].recv.head;
  }
  ctx->chs[ch].recv.tail->cord = braidcurr(b);

  return braidblock(b);
}

