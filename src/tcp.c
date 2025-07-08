#include <braid.h>
#include <braid/fd.h>
#include <braid/tcp.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>

static int ip(const char *name, uint32_t *out) {
  unsigned char addr[4];
  const char *p = name;
  int i;

  for (i = 0; i < 4 && *p; i++) {
    uchar x = strtoul(p, (char **)&p, 0);
    if (*p != '.' && *p != '\0') return -1;
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
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  if (resolve(host, &sa.sin_addr.s_addr) < 0) return -1;
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 && errno != EINPROGRESS) return -1;

  fdpoll(b, fd, POLLOUT);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &errlen) < 0) return -1;
  if (e != 0) {
    errno = e;
    return -1;
  }

  return fd;
}

int tcplisten(const char *host, int port) {
  int fd;
  struct sockaddr_in sa = {0};

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

  if (host != NULL && resolve(host, &sa.sin_addr.s_addr) < 0) return -1;

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) return -1;

  if (listen(fd, 16) < 0) return -1;

  return fd;
}

int tcpaccept(braid_t b, int fd) {
  int newfd, one = 1;

  fdpoll(b, fd, POLLIN);
  if ((newfd = accept(fd, NULL, NULL)) < 0) return -1;
  setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

  return newfd;
}

