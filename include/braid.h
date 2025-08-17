#ifndef _BRAID_H
#define _BRAID_H

#include <braid/types.h>
#include <stddef.h>
#include <stdarg.h>

#define CORD_NORMAL 0x00
#define CORD_SYSTEM 0x01

typedef struct braid * braid_t;
typedef uint64_t cord_t;

braid_t braidinit(void) __attribute__((malloc));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
cord_t  braidadd(braid_t b, void (*f)(), usize stacksize, const char *name, uchar flags, int nargs, ...);
cord_t  braidvadd(braid_t b, void (*f)(), usize stacksize, const char *name, uchar flags, int nargs, va_list args);
#pragma GCC diagnostic pop
void    braidstart(braid_t b);
void    braidyield(braid_t b);
usize   braidblock(braid_t b);
int     braidunblock(braid_t b, cord_t c, usize val);
void    braidstop(braid_t b);
void    braidexit(braid_t b);
cord_t  braidcurr(const braid_t b) __attribute__((pure));
uint    braidcnt(const braid_t b) __attribute__((pure));
uint    braidsys(const braid_t b) __attribute__((pure));
uint    braidblk(const braid_t b) __attribute__((pure));
void  **braiddata(braid_t b, uchar key) __attribute__((pure));
void    braidinfo(const braid_t b);

void    cordhalt(braid_t b, cord_t c);

void   *cordmalloc(braid_t b, cord_t c, size_t sz) __attribute__((malloc, alloc_size(3)));
void   *cordzalloc(braid_t b, cord_t c, size_t sz) __attribute__((malloc, alloc_size(3)));
void   *cordrealloc(braid_t b, cord_t c, void *p, size_t sz) __attribute__((alloc_size(4)));
void    cordfree(braid_t b, cord_t c, void *p);
void   *braidmalloc(braid_t b, size_t sz) __attribute__((malloc, alloc_size(2)));
void   *braidzalloc(braid_t b, size_t sz) __attribute__((malloc, alloc_size(2)));
void   *braidrealloc(braid_t b, void *p, size_t sz) __attribute__((alloc_size(3)));
void    braidfree(braid_t b, void *p);

#endif

