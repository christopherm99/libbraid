/* lambda.h - v0.1
 * Functional programming for C
 *
 * Copyright (c) 2025 Christopher Milan
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef LAMBDA_IMPLEMENTATION
#ifndef LAMBDA_DEFINITIONS
#define LAMBDA_DEFINITIONS
#define LAMBDA_USE_MPROTECT 0
#endif
#endif

#ifndef _LAMBDA_H
#define _LAMBDA_H

#include <stdint.h>

typedef struct {
  uintptr_t val;
  char  mov;
} arg_t;

#define LDR(x) ((arg_t){ x, 0 })
#define MOV(x) ((arg_t){ x, 1 })
#ifdef __aarch64__
#define LAMBDA_MAX(n) (12 * (n) + 8)
#define LAMBDA_SIZE(movs, ldrs, cycles) (4 * ((movs) + (cycles)) + 12 * (ldrs) + 16)
#define MAX_ARGS 8
#elif defined(__x86_64__)
#define LAMBDA_MAX(n) (6 * (n) + 12)
#define LAMBDA_SIZE(movs, ldrs, cycles) (3 * ((movs) + (cycles)) + 10 * (ldrs) + 12)
#define MAX_ARGS 6
#else
#error "unsupported architecture"
#endif

// creates a new function g from a function f, number of arguments n, and
// variadic arguments specified using the LDR and MOV macros. Arguments using
// LDR will inserted at the corresponding argument index, whereas arguments
// using MOV will move the data stored at g's argument index to the
// corresponding destination index. The maximum required size is given by
// LAMBDA_MAX(n) and the exact size can be computed with
// LAMBDA_SIZE(movs, ldrs, cycles), where cycles is the total number of disjoint
// cycles in the mov operations. The maximum value for n is given by MAX_ARGS.
uintptr_t (*lambda_bind(uintptr_t (*g)(), uintptr_t (*f)(), int n, ...))();

#endif
#ifdef LAMBDA_IMPLEMENTATION

#include <string.h>
#include <stdarg.h>
#ifdef LAMBDA_USE_MPROTECT
#include <sys/mman.h>
#endif

typedef unsigned char uchar;

#ifdef __aarch64__
#define TMP 16
#define mov(p,d,s) (p = (char *)memcpy(p, &(uint32_t){0xAA0003E0 + ((s) << 16) + (d)}, 4) + 4)
#define ldr(p,r,o) (p = (char *)memcpy(p, &(uint32_t){0x58000000 + ((o) << 3) + (r)}, 4) + 4)
#define br(p,r)    (p = (char *)memcpy(p, &(uint32_t){0xD61F0000 + ((r) << 5)}, 4) + 4)
#elif defined(__x86_64__)
#define TMP 6
#define u64(x) \
  (x >>  0) & 0xFF, (x >>  8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF, \
  (x >> 32) & 0xFF, (x >> 40) & 0xFF, (x >> 48) & 0xFF, (x >> 56) & 0xFF
#define r2i(r)      ((uchar []){7, 6, 2, 1, 0, 1, 0})[r]
#define rex(d,s)    (0x48 | ((s) > 3 && (s) != TMP) << 2 | ((d) > 3 && (s) != TMP))
#define modrm(d,s)  (0xC0 | r2i(s) << 3 | r2i(d))
#define mov(p,d,s)  (p = (char *)memcpy(p, (uchar []){rex(d,s), 0x89, modrm(d,s)}, 3) + 3)
#define movi(p,d,i) (p = (char *)memcpy(p, (uchar []){rex(d,0), 0xB8 + r2i(d), u64(i)}, 10) + 10)
#define j(p,i)      (p = (char *)memcpy(p, (uchar []){0x48, 0xB8, u64(i), 0xFF, 0xE0}, 12) + 12)
#else
#error "unsupported architecture"
#endif

enum { TO_MOVE, BEING_MOVED, MOVED };

// https://inria.hal.science/inria-00289709
static void move_one(void **p, int n, int i, int src[static n], const int dst[static n], char status[static n]) {
  if (src[i] != dst[i]) {
    status[i] = BEING_MOVED;
    for (int j = 0; j < n; j++)
      if (src[j] == dst[i])
        switch (status[j]) {
          case TO_MOVE: move_one(p, n, j, src, dst, status); break;
          case BEING_MOVED:
            mov(*p, TMP, src[j]);
            src[j] = TMP;
            break;
        }
    mov(*p, dst[i], src[i]);
    status[i] = MOVED;
  }
}

uintptr_t (*lambda_bind(uintptr_t (*g)(), uintptr_t (*f)(), int n, ...))() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
  void *p = (void *)g;
#pragma GCC diagnostic pop
  va_list args;
  int n_ldr = 0, n_mov = 0, msrc[MAX_ARGS] = {0}, mdst[MAX_ARGS] = {0}, ldst[MAX_ARGS] = {0};
  uintptr_t lsrc[MAX_ARGS] = {0};
  char status[MAX_ARGS] = { 0 };

  va_start(args, n);
  for (int i = 0; i < n; i++) {
    arg_t arg = va_arg(args, arg_t);
    if (arg.mov) {
      msrc[n_mov] = arg.val;
      mdst[n_mov++] = i;
    }
    else {
      lsrc[n_ldr] = arg.val;
      ldst[n_ldr++] = i;
    }
  }
  va_end(args);

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)g & ~0xFFF), LAMBDA_MAX(n), PROT_READ | PROT_WRITE)) return NULL;
#endif

  for (int i = 0; i < n_mov; i++)
    if (status[i] == TO_MOVE) move_one(&p, n_mov, i, msrc, mdst, status);

  {
#ifdef ldr
    uintptr_t *d = (uintptr_t *)((uint32_t *)p + (n - n_mov) + 2);
#endif
    for (int i = 0; i < n_ldr; i++) {
#ifdef ldr
      ldr(p, ldst[i], (uintptr_t)d - (uintptr_t)p);
      *d++ = lsrc[i];
#else
      movi(p, ldst[i], lsrc[i]);
#endif
    }

#ifdef br
    ldr(p, 16, (uintptr_t)d - (uintptr_t)p);
    br(p, 16);
    *d = (uintptr_t)f;
#else
    j(p, (uintptr_t)f);
#endif
  }

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)g & ~0xFFF), LAMBDA_MAX(n), PROT_READ | PROT_EXEC)) return NULL;
#endif

  return g;
}
#endif
