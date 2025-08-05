#ifndef _BRAID_CH_H
#define _BRAID_CH_H

#include <braid.h>

typedef uint64_t ch_t;

ch_t chcreate(void);
void chdestroy(ch_t ch);
int chsend(braid_t b, ch_t ch, usize data);
usize chrecv(braid_t b, ch_t ch);

#endif

