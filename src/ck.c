#include <braid.h>
#include <braid/ck.h>

#include <err.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>

#define alloc(x) calloc(1, x)

struct ckctx {
  struct elt {
    cord_t          cord;
    struct timespec ts;
    struct elt     *next;
  } *head;
};

static int tscmp(struct timespec a, struct timespec b) {
  if (a.tv_sec < b.tv_sec) return -1;
  if (a.tv_sec > b.tv_sec) return 1;
  if (a.tv_nsec < b.tv_nsec) return -1;
  if (a.tv_nsec > b.tv_nsec) return 1;
  return 0;
}

void ckvisor(braid_t b, usize _) {
  struct ckctx *ctx;
  (void)_;

  if ((ctx = *braiddata(b, BRAID_CK_KEY) = alloc(sizeof(struct ckctx))) == NULL) err(EX_OSERR, "ckvisor: alloc");

  for (;;) {
    if (ctx->head) {
      struct elt *e, *prev = NULL;
      struct timespec now;

      clock_gettime(CLOCK_REALTIME, &now);
      for (e = ctx->head; e; e = e->next) {
        if (tscmp(e->ts, now) <= 0) {
          braidunblock(b, e->cord, 0);
          if (prev) prev->next = e->next;
          else ctx->head = e->next;
          free(e);
        } else prev = e;
      }
    }
    braidyield(b);
  }
}

void ckwait(braid_t b, const struct timespec *ts) {
  struct ckctx **ctx = (struct ckctx **)braiddata(b, BRAID_CK_KEY);
  struct elt *e;

  while (!*ctx) braidyield(b);

  if ((e = alloc(sizeof(struct elt))) == NULL) err(EX_OSERR, "ckwait: alloc");
  e->cord = braidcurr(b);
  e->ts = *ts;
  e->next = (*ctx)->head;
  (*ctx)->head = e;

  braidblock(b);
}

void cknsleep(braid_t b, ulong ns) {
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_nsec += ns;
  ts.tv_sec += ts.tv_nsec / 1000000000L;
  ts.tv_nsec %= 1000000000L;
  
  ckwait(b, &ts);
}

void ckusleep(braid_t b, ulong us) { cknsleep(b, us * 1000L); }
void ckmsleep(braid_t b, ulong ms) { cknsleep(b, ms * 1000000L); }
void cksleep(braid_t b, ulong s) { cknsleep(b, s * 1000000000L); }

