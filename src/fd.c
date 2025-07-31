#include <braid.h>
#include <braid/fd.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define POLLIMPLICIT POLLERR | POLLHUP | POLLNVAL

struct fdctx {
  cord_t cord, *cords;
  struct pollfd *pfds;
  uint cnt, cap;
  char is_blocked;
};

static void blocked_append(struct fdctx *ctx, cord_t c, struct pollfd *pfd) {
  if (ctx->cnt + 1 > ctx->cap) {
    ctx->cap = (ctx->cap == 0) ? 4 : ctx->cap * 2;
    if ((ctx->cords = realloc(ctx->cords, ctx->cap * sizeof(cord_t))) == NULL ||
        (ctx->pfds = realloc(ctx->pfds, ctx->cap * sizeof(struct pollfd))) == NULL)
      err(EX_OSERR, "blocked_append: realloc");
  }
  ctx->cords[ctx->cnt] = c;
  ctx->pfds[ctx->cnt++] = *pfd;
}

static void blocked_remove(struct fdctx *ctx, cord_t c) {
  uint i;
  for (i = 0; i < ctx->cnt; i++) {
    if (ctx->cords[i] == c) {
      memmove(&ctx->cords[i], &ctx->cords[i + 1], (ctx->cnt - i - 1) * sizeof(cord_t));
      memmove(&ctx->pfds[i], &ctx->pfds[i + 1], (ctx->cnt - i - 1) * sizeof(struct pollfd));
      ctx->cnt--;
      return;
    }
  }
  warnx("blocked_remove: cord not found");
}

void fdvisor(braid_t b, usize _) {
  struct fdctx *ctx;
  (void)_;

  if ((ctx = *braiddata(b, BRAID_FD_KEY) = cordzalloc(b, sizeof(struct fdctx))) == NULL) err(EX_OSERR, "fdvisor: alloc");
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
        uint i;
        for (i = 0; i < ctx->cnt; i++) {
          if (ctx->pfds[i].revents & (POLLIMPLICIT | ctx->pfds[i].events)) {
            braidunblock(b, ctx->cords[i], ctx->pfds[i].revents);
            blocked_remove(ctx, ctx->cords[i--]);
          }
        }
      }
      braidyield(b);
    } else { ctx->is_blocked = 1; braidblock(b); }
  }
}

short fdpoll(braid_t b, int fd, short events) {
  struct fdctx **ctx = (struct fdctx **)braiddata(b, BRAID_FD_KEY);
  struct pollfd p;

  /* wait until fdvisor has run */
  while (!*ctx) braidyield(b);

  p.fd = fd;
  p.events = events;
  blocked_append(*ctx, braidcurr(b), &p);
  if ((*ctx)->is_blocked) {
    braidunblock(b, (*ctx)->cord, 0);
    (*ctx)->is_blocked = 0;
  }

  return (short)braidblock(b);
}

int fdread(braid_t b, int fd, void *buf, size_t count) {
  fdpoll(b, fd, POLLIN);
  return read(fd, buf, count);
}

int fdwrite(braid_t b, int fd, const void *buf, size_t count) {
  fdpoll(b, fd, POLLOUT);
  return write(fd, buf, count);
}

