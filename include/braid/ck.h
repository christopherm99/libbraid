#ifndef _BRAID_CK_H
#define _BRAID_CK_H

#include <braid.h>

#include <time.h>

#define BRAID_CK_KEY 0xFE

void ckvisor(braid_t b, usize _);
void ckwait(braid_t b, const struct timespec *ts);
void cknsleep(braid_t b, ulong ns);
void ckusleep(braid_t b, ulong us);
void ckmsleep(braid_t b, ulong ms);
void cksleep(braid_t b, ulong s);

#endif

