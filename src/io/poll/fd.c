#include <braid.h>
#include <braid/fd.h>

#include <poll.h>
#include <unistd.h>

extern void poll_append(braid_t b, struct pollfd *pfd);

void fdwait(braid_t b, int fd, char rw) {
  struct pollfd p = { .fd = fd, .events = rw == 'r' ? POLLOUT : POLLIN };

  poll_append(b, &p);
  braidblock(b);
}

int fdread(braid_t b, int fd, void *buf, size_t count) {
  fdwait(b, fd, 'r');
  return read(fd, buf, count);
}

int fdwrite(braid_t b, int fd, const void *buf, size_t count) {
  fdwait(b, fd, 'w');
  return write(fd, buf, count);
}

