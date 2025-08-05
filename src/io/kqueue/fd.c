#include <braid.h>
#include <braid/fd.h>

#include <sys/event.h>
#include <unistd.h>

extern void kev_append(braid_t b, uintptr_t ident, int16_t filter, uint32_t fflags, int64_t data);

void fdwait(braid_t b, int fd, char rw) {
  if (rw != 'r' && rw != 'w') return;
  kev_append(b, fd, rw == 'r' ? EVFILT_READ : EVFILT_WRITE, 0, 0);
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

