#ifndef _BRAID_H
#define _BRAID_H

#include <braid/types.h>

#define CORD_NORMAL 0x00
#define CORD_SYSTEM 0x01

typedef struct braid * braid_t;
typedef struct cord * cord_t;

braid_t braidinit(void);
void    braidadd(braid_t b, void (*f)(braid_t, usize), usize stacksize, const char *name, uchar flags, usize arg);
void    braidstart(braid_t b);
void    braidyield(braid_t b);
usize   braidblock(braid_t b);
void    braidunblock(braid_t b, cord_t c, usize val);
void    braidexit(braid_t b);
cord_t  braidcurr(braid_t b);
uint    braidcnt(braid_t b);
void  **braiddata(braid_t b, uchar key);

#endif

