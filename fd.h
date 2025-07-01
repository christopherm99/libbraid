#ifndef _FD_H
#define _FD_H

#include "braid.h"
#include <stddef.h>

#define BRAID_FD_KEY 0xFD

void fdvisor(braid_t b, usize _);
short fdpoll(braid_t b, int fd, short events);
int fdread(braid_t b, int fd, void *buf, size_t count);
int fdwrite(braid_t b, int fd, const void *buf, size_t count);

#endif

