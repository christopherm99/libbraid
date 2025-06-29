#ifndef _BRAID_H
#define _BRAID_H
#include <stdint.h>

typedef uintptr_t usize;
typedef unsigned char uchar;
typedef void* cord_t;

cord_t cordactive(void);
cord_t cordcreate(void (*f)(void), usize stacksize);
void cordswitch(cord_t c);

#endif

