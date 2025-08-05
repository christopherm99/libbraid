#define _GNU_SOURCE
#include <braid.h>
#include <braid/fd.h>
#include <braid/ck.h>

#include <liburing.h>

extern struct io_uring_sqe *get_sqe(braid_t b);

void cknsleep(braid_t b, ulong ns) {
  struct __kernel_timespec ts = { .tv_sec = ns / 1000000000L, .tv_nsec = ns % 1000000000L };
  io_uring_prep_timeout(get_sqe(b), &ts, 0, 0);
  braidblock(b);
}

void ckusleep(braid_t b, ulong us) { cknsleep(b, us * 1000L); }
void ckmsleep(braid_t b, ulong ms) { cknsleep(b, ms * 1000000L); }
void cksleep(braid_t b, ulong s) { cknsleep(b, s * 1000000000L); }

