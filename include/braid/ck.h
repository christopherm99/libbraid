#ifndef _BRAID_CK_H
#define _BRAID_CK_H

#include <braid.h>

void cknsleep(braid_t b, ulong ns);
void ckusleep(braid_t b, ulong us);
void ckmsleep(braid_t b, ulong ms);
void cksleep(braid_t b, ulong s);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
usize ckntimeoutv(braid_t b, usize (*f)(), usize stacksize, ulong ns, int nargs, va_list args);
usize ckutimeoutv(braid_t b, usize (*f)(), usize stacksize, ulong us, int nargs, va_list args);
usize ckmtimeoutv(braid_t b, usize (*f)(), usize stacksize, ulong ms, int nargs, va_list args);
usize cktimeoutv(braid_t b, usize (*f)(), usize stacksize, ulong s, int nargs, va_list args);

usize ckntimeout(braid_t b, usize (*f)(), usize stacksize, ulong ns, int nargs, ...);
usize ckutimeout(braid_t b, usize (*f)(), usize stacksize, ulong us, int nargs, ...);
usize ckmtimeout(braid_t b, usize (*f)(), usize stacksize, ulong ms, int nargs, ...);
usize cktimeout(braid_t b, usize (*f)(), usize stacksize, ulong s, int nargs, ...);
#pragma GCC diagnostic pop

#endif

