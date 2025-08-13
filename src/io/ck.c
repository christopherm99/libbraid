#include <braid.h>
#include <braid/ck.h>
#include <braid/ch.h>

#include <err.h>
#include <stdarg.h>
#include <sysexits.h>
#include <sys/mman.h>

#include "../lambda.h"

#define varg(name) \
  usize name(braid_t b, fn_t f, usize stacksize, ulong t, int nargs, ...) { \
    va_list args; \
    usize ret; \
    va_start(args, nargs); \
    ret = name##v(b, f, stacksize, t, nargs, args); \
    va_end(args); \
    return ret; \
  }

usize ckntimeoutv(braid_t b, fn_t f, usize stacksize, ulong ns, int nargs, va_list args) {
  usize ret;
  struct fns {
    uchar work[LAMBDA_COMPOSE_SIZE];
    uchar send1[LAMBDA_BIND_SIZE(1,2,0)];
    uchar wait[LAMBDA_COMPOSE_SIZE];
    uchar send2[LAMBDA_BIND_SIZE(0,3,0)];
  } *fns;
  ch_t c = chcreate();
  cord_t cwork, cwait;

  if ((fns = (struct fns *)mmap(NULL, sizeof(struct fns), PROT_NONE, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED)
    err(EX_OSERR, "timeout: mmap");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
  cwork = braidvadd(b, (void (*)(usize))
      lambda_compose(fns->work,
        lambda_bind(fns->send1, (fn_t)chsend, 3, LDR((usize)b), LDR((usize)c), MOV(0)),
        f),
      stacksize, "timeout (work)", CORD_NORMAL, nargs, args);
  cwait = braidadd(b, (void (*)(braid_t b, ulong))
      lambda_compose(fns->wait,
        lambda_bindldr(fns->send2, (fn_t)chsend, 0, 3, b, c, 0),
        (fn_t)cknsleep),
      65536, "timeout (wait)", CORD_NORMAL, 2, b, ns);
#pragma GCC diagnostic pop

  if ((ret = chrecv(b, c))) cordhalt(b, cwait);
  else cordhalt(b, cwork);

  chdestroy(c);
  munmap(fns, sizeof(struct fns));

  return ret;
}

usize ckutimeoutv(braid_t b, fn_t f, usize stacksize, ulong us, int nargs, va_list args) { return ckntimeoutv(b, f, stacksize, us * 1000L, nargs, args); }
usize ckmtimeoutv(braid_t b, fn_t f, usize stacksize, ulong ms, int nargs, va_list args) { return ckntimeoutv(b, f, stacksize, ms * 1000000L, nargs, args); }
usize cktimeoutv(braid_t b, fn_t f, usize stacksize, ulong s, int nargs, va_list args) { return ckntimeoutv(b, f, stacksize, s * 1000000000L, nargs, args); }

varg(ckntimeout)
varg(ckutimeout)
varg(ckmtimeout)
varg(cktimeout)

