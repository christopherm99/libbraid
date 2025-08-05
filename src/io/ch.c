#include <braid.h>
#include <braid/fd.h>
#include <braid/ch.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef union {
  ch_t ch;
  int fd[2];
} ch_u;

static int read_all(int fd, void *buf, size_t len) {
  char *p = buf;
  while (len) {
    ssize_t n = read(fd, p, len);
    if (n <= 0) return -1;
    p += n;
    len -= n;
  }
  return 0;
}

ch_t chcreate(void) {
  ch_u c;
  if ((socketpair(AF_UNIX, SOCK_SEQPACKET, 0, c.fd) && errno != EPROTONOSUPPORT) ||
      socketpair(AF_UNIX, SOCK_STREAM, 0, c.fd))
    return 0;

  fcntl(c.fd[0], F_SETFL, fcntl(c.fd[0], F_GETFL) | FD_CLOEXEC);
  fcntl(c.fd[1], F_SETFL, fcntl(c.fd[1], F_GETFL) | FD_CLOEXEC);
  return c.ch;
}

void chdestroy(ch_t _ch) {
  ch_u ch = { _ch };
  close(ch.fd[0]);
  close(ch.fd[1]);
}

int chsend(braid_t b, ch_t _ch, usize data) {
  ch_u ch = { _ch };
  char ack;
  if (send(ch.fd[0], &data, sizeof(data), 0) != sizeof(data)) return -1;
  fdwait(b, ch.fd[0], 'r');
  if (recv(ch.fd[0], &ack, 1, 0) != 1) return -1;
  return 0;
}

usize chrecv(braid_t b, ch_t _ch) {
  ch_u ch = { _ch };
  usize data;
  fdwait(b, ch.fd[1], 'r');
  if (read_all(ch.fd[1], &data, sizeof(data))) return -1;
  send(ch.fd[1], &(char){0}, 1, 0);
  return data;
}

