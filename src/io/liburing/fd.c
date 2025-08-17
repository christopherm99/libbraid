#define _GNU_SOURCE
#include <braid.h>
#include <braid/fd.h>

#include "../../valgrind/memcheck.h"

#include <liburing.h>
#include <poll.h>

extern struct io_uring_sqe *get_sqe(braid_t b);

void fdwait(braid_t b, int fd, char rw) {
  io_uring_prep_poll_add(get_sqe(b), fd, rw == 'r' ? POLLIN : POLLOUT);
  braidblock(b);
}

int fdread(braid_t b, int fd, void *buf, size_t count) {
  /* FIXME: two braids reading from the same file may cause issues */
  io_uring_prep_read(get_sqe(b), fd, buf, count, -1);
  /* https://bugs.kde.org/show_bug.cgi?id=508319 */
  VALGRIND_MAKE_MEM_DEFINED(buf, count);
  return braidblock(b);
}

int fdwrite(braid_t b, int fd, const void *buf, size_t count) {
  /* FIXME: two braids writing to the same file may cause issues */
  io_uring_prep_write(get_sqe(b), fd, buf, count, -1);
  return braidblock(b);
}

