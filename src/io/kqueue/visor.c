#include <braid.h>
#include <braid/io.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>
#include <sys/event.h>
#include <sys/time.h>

#define IMMEDIATE &(struct timespec){0}
#define INFINITE 0

struct ioctx {
  int kq, is_blocked;
  uint cap, cnt;
  cord_t cord;
  struct kevent *kevs;
};

static struct ioctx *getctx(braid_t b) {
  struct ioctx **ctx;
  if (!*(ctx = (struct ioctx **)braiddata(b, BRAID_IO_KEY)))
    /* TODO: this should be in the cord's lifetime */
    if (!(*ctx = braidzalloc(b, sizeof(struct ioctx)))) err(EX_OSERR, "iovisor: alloc");
  return *ctx;
}

void kev_append(braid_t b, uintptr_t ident, int16_t filter, uint32_t fflags, int64_t data) {
  struct kevent kev;
  struct ioctx *ctx = getctx(b);

  /* FIXME: ONESHOT is not ideal */
  EV_SET(&kev, ident, filter, EV_ADD | EV_ONESHOT, fflags, data, braidcurr(b));

  if (ctx->cnt + 1 > ctx->cap) {
    ctx->cap = (ctx->cap == 0) ? 4 : ctx->cap * 2;
    if ((ctx->kevs = cordrealloc(ctx->cord, ctx->kevs, ctx->cap * sizeof(struct kevent))) == NULL)
      err(EX_OSERR, "kev_append: realloc");
  }
  ctx->kevs[ctx->cnt++] = kev;

  if (ctx->is_blocked) { braidunblock(b, ctx->cord, 0); ctx->is_blocked = 0; }
}

static void kev_remove(struct ioctx *ctx, cord_t c) {
  uint i;
  for (i = 0; i < ctx->cnt; i++) {
    if (ctx->kevs[i].udata == c) {
      memmove(&ctx->kevs[i], &ctx->kevs[i + 1], (ctx->cnt - i - 1) * sizeof(struct kevent));
      ctx->cnt--;
      return;
    }
  }
}

void iovisor(braid_t b) {
  struct ioctx *ctx = getctx(b);

  if ((ctx->kq = kqueue()) == -1) err(EX_OSERR, "iovisor: kqueue");
  ctx->cord = braidcurr(b);

  for (;;) {
    /* if we're the only cord (system or normal), then we can wait indefinitely */
    /* FIXME: this may be false in pthread scenarios? */
    if (ctx->cnt) {
      int rc;
      struct kevent *evs = cordmalloc(ctx->cord, sizeof(struct kevent) * ctx->cnt);
      if ((rc = kevent(ctx->kq, ctx->kevs, ctx->cnt, evs, ctx->cnt, (braidcnt(b) + braidsys(b)) ? IMMEDIATE : INFINITE)) == -1 && errno != EINTR)
        err(EX_OSERR, "iovisor: kevent");
      for (int i = 0; i < rc; i++) {
        kev_remove(ctx, evs[i].udata);
        braidunblock(b, evs[i].udata, evs[i].data);
      }
      cordfree(ctx->cord, evs);
      braidyield(b);
    } else {
      ctx->is_blocked = 1;
      braidblock(b);
    }
  }
}

