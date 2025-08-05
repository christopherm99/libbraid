#include <braid.h>
#include <braid/ck.h>

#include <sys/event.h>

extern void kev_append(braid_t b, uintptr_t ident, int16_t filter, uint32_t fflags, int64_t data);

/* TODO: what if someone creates 2^64 timers?!?? */
static uintptr_t ident = 0;

void cknsleep(braid_t b, ulong ns) {
  kev_append(b, ident++, EVFILT_TIMER, NOTE_NSECONDS, ns);
  braidblock(b);
}

void ckusleep(braid_t b, ulong us) {
  kev_append(b, ident++, EVFILT_TIMER, NOTE_USECONDS, us);
  braidblock(b);
}

void ckmsleep(braid_t b, ulong ms) {
  kev_append(b, ident++, EVFILT_TIMER, 0, ms);
  braidblock(b);
}

void cksleep(braid_t b, ulong s) {
  kev_append(b, ident++, EVFILT_TIMER, NOTE_SECONDS, s);
  braidblock(b);
}

