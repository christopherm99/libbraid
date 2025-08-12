#define _GNU_SOURCE
#include <braid.h>
#include <braid/io.h>

#include <err.h>
#include <liburing.h>
#include <sysexits.h>

#define IMM &(struct __kernel_timespec){0}
#define INF 0

struct ioctx {
  cord_t cord;
  struct io_uring ring;
  uint cnt;
  char is_blocked;
};

static struct ioctx *getctx(braid_t b) {
  struct ioctx **ctx;
  if (!*(ctx = (struct ioctx **)braiddata(b, BRAID_IO_KEY))) {
    /* TODO: this should be in the cord's lifetime */
    if (!(*ctx = braidzalloc(b, sizeof(struct ioctx)))) err(EX_OSERR, "iovisor: alloc");
    /* FIXME: choose a reasonable value for entries */
    if (io_uring_queue_init(32, &(*ctx)->ring, IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN))
      err(EX_OSERR, "iovisor: io_uring_setup");
  }
  return *ctx;
}

struct io_uring_sqe *get_sqe(braid_t b) {
  struct ioctx *ctx = getctx(b);
  struct io_uring_sqe *sqe = io_uring_get_sqe(&ctx->ring);
  io_uring_sqe_set_data(sqe, braidcurr(b));
  ctx->cnt++;
  if (ctx->is_blocked) { braidunblock(b, ctx->cord, 0); ctx->is_blocked = 0; }
  return sqe;
}

void iovisor(braid_t b) {
  struct ioctx *ctx = getctx(b);

  ctx->cord = braidcurr(b);

  for (;;) {
    if (ctx->cnt) {
      uint head, i = 0;
      struct io_uring_cqe *cqe;

      /* TODO: should we submit in the cords? */
      if (io_uring_submit_and_wait_timeout(&ctx->ring, &cqe, 1, (braidcnt(b) + braidsys(b)) ? IMM : INF, NULL) < 0) {
        if (errno == -EINTR) continue;
        if (errno == -ETIME) goto yield;
        err(EX_OSERR, "iovisor: io_uring_submit_and_wait");
      }

      io_uring_for_each_cqe(&ctx->ring, head, cqe) {
        braidunblock(b, io_uring_cqe_get_data(cqe), cqe->res);
        i++;
      }

      if (i) io_uring_cq_advance(&ctx->ring, i);
      ctx->cnt -= i;
yield:
      braidyield(b);
    } else {
      ctx->is_blocked = 1;
      braidblock(b);
    }
  }
}

