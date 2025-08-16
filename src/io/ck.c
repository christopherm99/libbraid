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

#ifdef __APPLE__
#define MMAP_PROT PROT_READ | PROT_WRITE
#else
#define MMAP_PROT PROT_READ | PROT_WRITE | PROT_EXEC
#endif

static void work(braid_t b, fn_t f, ch_t ch) { chsend(b, ch, f()); }

static void wait(braid_t b, cord_t c, ch_t ch, ulong ns) {
  cknsleep(b, ns);
  cordhalt(b, c);
  chclose(b, ch);
}

usize ckntimeoutv(braid_t b, fn_t f, usize stacksize, ulong ns, int nargs, va_list args) {
  char ok;
  usize ret;
  uchar *bound_f;
  ch_t c = chopen(b);
  cord_t cwork, cwait;

  if ((bound_f = mmap(NULL, LAMBDA_BIND_SIZE(0,nargs,0), MMAP_PROT, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED)
    err(EX_OSERR, "timeout: mmap");

  cwork = braidadd(b, work, stacksize, "timeout (work)", CORD_NORMAL, 3, b, lambda_vbindldr(bound_f, f, 0, nargs, args), c);
  cwait = braidadd(b, wait, 65536, "timeout (wait)", CORD_NORMAL, 4, b, cwork, c, ns);

  if ((ret = chrecv(b, c, &ok)), ok) {
    cordhalt(b, cwait);
    chclose(b, c);
  }

  munmap(bound_f, LAMBDA_BIND_SIZE(0,nargs,0));

  return ret;
}

usize ckutimeoutv(braid_t b, fn_t f, usize stacksize, ulong us, int nargs, va_list args) { return ckntimeoutv(b, f, stacksize, us * 1000L, nargs, args); }
usize ckmtimeoutv(braid_t b, fn_t f, usize stacksize, ulong ms, int nargs, va_list args) { return ckntimeoutv(b, f, stacksize, ms * 1000000L, nargs, args); }
usize cktimeoutv(braid_t b, fn_t f, usize stacksize, ulong s, int nargs, va_list args) { return ckntimeoutv(b, f, stacksize, s * 1000000000L, nargs, args); }

varg(ckntimeout)
varg(ckutimeout)
varg(ckmtimeout)
varg(cktimeout)

