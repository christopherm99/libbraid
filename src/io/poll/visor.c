#include <braid.h>
#include <braid/io.h>

#include <errno.h>
#include <err.h>
#include <poll.h>
#include <string.h>
#include <sysexits.h>

#define POLLIMPLICIT POLLERR | POLLHUP | POLLNVAL

struct ioctx {
  cord_t cord, *cords;
  struct pollfd *pfds;
  uint cnt, cap;
  char is_blocked;
};

static struct ioctx *getctx(braid_t b) {
  struct ioctx **ctx;
  if (!*(ctx = (struct ioctx **)braiddata(b, BRAID_IO_KEY)))
    /* TODO: this should be in the cord's lifetime */
    if (!(*ctx = braidzalloc(b, sizeof(struct ioctx)))) err(EX_OSERR, "iovisor: alloc");
  return *ctx;
}

void poll_append(braid_t b, struct pollfd *pfd) {
  struct ioctx *ctx = getctx(b);
  if (ctx->cnt + 1 > ctx->cap) {
    ctx->cap = (ctx->cap == 0) ? 4 : ctx->cap * 2;
    if ((ctx->cords = cordrealloc(ctx->cord, ctx->cords, ctx->cap * sizeof(cord_t))) == NULL ||
        (ctx->pfds = cordrealloc(ctx->cord, ctx->pfds, ctx->cap * sizeof(struct pollfd))) == NULL)
      err(EX_OSERR, "poll_append: realloc");
  }
  ctx->cords[ctx->cnt] = braidcurr(b);
  ctx->pfds[ctx->cnt++] = *pfd;
  if (ctx->is_blocked) { braidunblock(b, ctx->cord, 0); ctx->is_blocked = 0; }
}

static void poll_remove(braid_t b, cord_t c) {
  struct ioctx *ctx = getctx(b);
  for (uint i = 0; i < ctx->cnt; i++) {
    if (ctx->cords[i] == c) {
      memmove(&ctx->cords[i], &ctx->cords[i + 1], (ctx->cnt - i - 1) * sizeof(cord_t));
      memmove(&ctx->pfds[i], &ctx->pfds[i + 1], (ctx->cnt - i - 1) * sizeof(struct pollfd));
      ctx->cnt--;
      return;
    }
  }
  warnx("poll_remove: cord not found");
}

void iovisor(braid_t b) {
  struct ioctx *ctx = getctx(b);

  ctx->cord = braidcurr(b);

  for (;;) {
    if (ctx->cnt) {
      int rc;
      /* if we're the only cord (system or normal), then we can poll indefinitely */
      /* FIXME: this may be false in pthread scenarios? */
      if ((rc = poll(ctx->pfds, ctx->cnt, (braidcnt(b) + braidsys(b)) ? 0 : -1)) < 0 && errno != EINTR)
        err(EX_OSERR, "fdvisor: poll");
      if (rc == 0) braidyield(b);
      else {
        uint i = 0;
        while (i < ctx->cnt) {
          if (ctx->pfds[i].revents & (POLLIMPLICIT | ctx->pfds[i].events)) {
            braidunblock(b, ctx->cords[i], ctx->pfds[i].revents);
            poll_remove(b, ctx->cords[i]);
          } else i++;
        }
      }
      braidyield(b);
    } else { ctx->is_blocked = 1; braidblock(b); }
  }
}

