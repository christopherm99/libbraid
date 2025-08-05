#include <braid.h>
#include <braid/fd.h>
#include <braid/ck.h>

#include <err.h>
#include <sysexits.h>
#include <sys/timerfd.h>
#include <unistd.h>

/* TODO: use a min-heap on the poll timeout rather than timerfds */

void cknsleep(braid_t b, ulong ns) {
  int tfd;
  struct itimerspec its = { .it_value = {.tv_sec = ns / 1000000000L, .tv_nsec = ns % 1000000000L} };

  if ((tfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC)) == -1) err(EX_OSERR, "timerfd_create");
  if (timerfd_settime(tfd, 0, &its, 0)) err(EX_OSERR, "timerfd_settime");

  fdwait(b, tfd, 'r');
  close(tfd);
}

void ckusleep(braid_t b, ulong us) { cknsleep(b, us * 1000L); }
void ckmsleep(braid_t b, ulong ms) { cknsleep(b, ms * 1000000L); }
void cksleep(braid_t b, ulong s) { cknsleep(b, s * 1000000000L); }

