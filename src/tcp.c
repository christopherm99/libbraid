#include <braid.h>
#include <braid/fd.h>
#include <braid/tcp.h>

#include <netinet/tcp.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

static int ip(const char *name, uint32_t *out) {
  unsigned char addr[4];
  const char *p = name;
  int i;

  for (i = 0; i < 4 && *p; i++) {
    uchar x;
    if ((x = strtoul(p, (char **)&p, 0)) > 255 || (*p != '.' && *p != '\0')) return -1;
    if (*p == '.') p++;
    addr[i] = x;
  }

  if (i != 4 || *p != '\0') return -1;
  *out = *(uint32_t *)addr;
  return 0;
}

static int resolve(const char *name, uint32_t *out) {
  struct hostent *he;

  if (ip(name, out) == 0) return 0;
  if ((he = gethostbyname(name)) == NULL) return -1;
  *out = *(uint32_t *)he->h_addr_list[0];
  return 0;
}

int tcpdial(braid_t b, int fd, const char *host, int port) {
  int e;
  struct sockaddr_in sa = {0};
  socklen_t errlen = sizeof(e);

  if (fd < 0)
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) err(EX_OSERR, "fddial: socket");

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  if (resolve(host, &sa.sin_addr.s_addr) < 0) err(EX_OSERR, "fddial: resolve");
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 && errno != EINPROGRESS) err(EX_OSERR, "fddial: connect");

  fdpoll(b, fd, POLLOUT);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &errlen) < 0) err(EX_OSERR, "fddial: getsockopt");
  if (e != 0) errx(EX_OSERR, "fddial: connect: %s", strerror(e));

  return fd;
}

int tcplisten(const char *host, int port) {
  int fd;
  struct sockaddr_in sa = {0};

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) err(EX_OSERR, "fdlisten: socket");

  if (host != NULL && resolve(host, &sa.sin_addr.s_addr) < 0) err(EX_OSERR, "fdlisten: resolve");

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) err(EX_OSERR, "fdlisten: bind");

  if (listen(fd, 16) < 0) err(EX_OSERR, "fdlisten: listen");

  return fd;
}

int tcpaccept(braid_t b, int fd) {
  int newfd, one = 1;

  fdpoll(b, fd, POLLIN);
  if ((newfd = accept(fd, NULL, NULL)) < 0) err(EX_OSERR, "fdaccept: accept");
  setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

  return newfd;
}

