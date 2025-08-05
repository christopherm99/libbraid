#ifndef _BRAID_FD_H
#define _BRAID_FD_H

#include <braid.h>
#include <stddef.h>

void fdwait(braid_t b, int fd, char rw);
int fdread(braid_t b, int fd, void *buf, size_t count);
int fdwrite(braid_t b, int fd, const void *buf, size_t count);

#endif

