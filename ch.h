#ifndef _BRAID_CH_H
#define _BRAID_CH_H

#include "braid.h"

#define BRAID_CH_KEY 0xFC

typedef uint ch_t;

void chvisor(braid_t b, usize _);
ch_t chcreate(braid_t b);
void chdestroy(braid_t b, ch_t ch);
uint chsend(braid_t b, ch_t ch, usize data);
usize chrecv(braid_t b, ch_t ch);

#endif

