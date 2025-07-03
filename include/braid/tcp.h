#ifndef _BRAID_TCP_H
#define _BRAID_TCP_H

#include <braid.h>

int tcpdial(braid_t b, const char *host, int port);
int tcplisten(const char *host, int port);
int tcpaccept(braid_t b, int fd);

#endif

