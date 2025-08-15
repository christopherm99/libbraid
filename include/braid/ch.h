#ifndef _BRAID_CH_H
#define _BRAID_CH_H

#include <braid.h>

typedef struct ch * ch_t;

ch_t chopen(braid_t b);
void chclose(braid_t b, ch_t ch);
int chsend(braid_t b, ch_t ch, usize data);
usize chrecv(braid_t b, ch_t ch, char *ok);

#endif

