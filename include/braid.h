#ifndef _BRAID_H
#define _BRAID_H

#include <braid/types.h>
#include <stddef.h>

#define CORD_NORMAL 0x00
#define CORD_SYSTEM 0x01

typedef struct braid * braid_t;
typedef struct cord * cord_t;

braid_t braidinit(void);
cord_t  braidadd(braid_t b, void (*f)(braid_t, usize), usize stacksize, const char *name, uchar flags, usize arg);
void    braidstart(braid_t b);
void    braidyield(braid_t b);
usize   braidblock(braid_t b);
int     braidunblock(braid_t b, cord_t c, usize val);
void    braidstop(braid_t b);
void    braidexit(braid_t b);
cord_t  braidcurr(braid_t b);
uint    braidcnt(braid_t b);
uint    braidsys(braid_t b);
void  **braiddata(braid_t b, uchar key);
void    braidinfo(braid_t b);

usize  *cordarg(cord_t c);
void    cordhalt(braid_t b, cord_t c);

void   *cordmalloc(cord_t c, size_t sz);
void   *cordzalloc(cord_t c, size_t sz);
void   *cordrealloc(cord_t c, void *p, size_t sz);
void    cordfree(cord_t c, void *p);
void   *braidmalloc(braid_t b, size_t sz);
void   *braidzalloc(braid_t b, size_t sz);
void   *braidrealloc(braid_t b, void *p, size_t sz);
void    braidfree(braid_t b, void *p);

#endif

