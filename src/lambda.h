/* lambda.h - v0.2
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
#include <stdarg.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
typedef uintptr_t (*fn_t)();
#pragma GCC diagnostic pop

typedef struct {
  uintptr_t val;
  char  mov;
} arg_t;

#define LDR(x) ((arg_t){ x, 0 })
#define MOV(x) ((arg_t){ x, 1 })
#ifdef __aarch64__
#define LAMBDA_BIND_MAX_SIZE(n) (12 * (n) + 8)
#define LAMBDA_BIND_SIZE(movs, ldrs, cycles) (4 * ((movs) + (cycles)) + 12 * (ldrs) + 16)
#define LAMBDA_BIND_MAX_ARGS 8
#elif defined(__x86_64__)
#define LAMBDA_BIND_MAX_SIZE(n) (6 * (n) + 12)
#define LAMBDA_BIND_SIZE(movs, ldrs, cycles) (3 * ((movs) + (cycles)) + 10 * (ldrs) + 12)
#define LAMBDA_BIND_MAX_ARGS 6
#else
#error "unsupported architecture"
#endif

// Creates a new function g from a function f, number of arguments n, and
// variadic arguments specified using the LDR and MOV macros. Arguments using
// LDR will inserted at the corresponding argument index, whereas arguments
// using MOV will move the data stored at g's argument index to the
// corresponding destination index. The maximum required size is given by
// LAMBDA_BIND_MAX_SIZE(n) and the exact size can be computed with
// LAMBDA_BIND_SIZE(movs, ldrs, cycles), where cycles is the total number of
// disjoint cycles in the mov operations. The maximum value for n is given by
// LAMBDA_BIND_MAX_ARGS.
fn_t lambda_bind(void *g, fn_t f, int n, ...);
fn_t lambda_vbind(void *g, fn_t f, int n, va_list args);

// the same as lambda_bind, but all arguments are automatically interpreted as
// LDR, without requiring the wrapper macro. Start specifies the destination
// index to start at, and arguments are inserted sequentially. LAMBDA_BIND_MAX_ARGS corresponds
// to the maximum value of start + n.
fn_t lambda_bindldr(void *g, fn_t f, int start, int n, ...);
fn_t lambda_vbindldr(void *g, fn_t f, int start, int n, va_list args);

#ifdef __aarch64__
#define LAMBDA_COMPOSE_SIZE 40
#elif defined(__x86_64__)
#define LAMBDA_COMPOSE_SIZE 27
#endif

// Creates a new function h from the functions f and g by composing f with g,
// ie. h(x) = f(g(x)). The required size for h is given by LAMBDA_COMPOSE_SIZE.
fn_t lambda_compose(void *h, fn_t f, fn_t g);

#endif
#ifdef LAMBDA_IMPLEMENTATION

#include <string.h>
#include <unistd.h>
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

fn_t lambda_bind(void *g, fn_t f, int n, ...) {
  va_list args;
  fn_t ret;
  va_start(args, n);
  ret = lambda_vbind(g, f, n, args);
  va_end(args);
  return ret;
}

fn_t lambda_vbind(void *g, fn_t f, int n, va_list args) {
  void *p = g;
  int n_ldr = 0, n_mov = 0, msrc[LAMBDA_BIND_MAX_ARGS] = {0}, mdst[LAMBDA_BIND_MAX_ARGS] = {0}, ldst[LAMBDA_BIND_MAX_ARGS] = {0};
  uintptr_t lsrc[LAMBDA_BIND_MAX_ARGS] = {0};
  char status[LAMBDA_BIND_MAX_ARGS] = { 0 };
#if LAMBDA_USE_MPROTECT
  long pagemask = ~(sysconf(_SC_PAGESIZE) - 1);
#endif

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

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)g & pagemask), LAMBDA_BIND_MAX_SIZE(n), PROT_READ | PROT_WRITE)) return NULL;
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

  __builtin___clear_cache(g, (char *)g + LAMBDA_BIND_MAX_SIZE(n));

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)g & pagemask), LAMBDA_BIND_MAX_SIZE(n), PROT_READ | PROT_EXEC)) return NULL;
#endif

  return (fn_t)g;
}

fn_t lambda_bindldr(void *g, fn_t f, int start, int n, ...) {
  va_list args;
  fn_t ret;
  va_start(args, n);
  ret = lambda_vbindldr(g, f, start, n, args);
  va_end(args);
  return ret;
}

fn_t lambda_vbindldr(void *g, fn_t f, int start, int n, va_list _args) {
  void *p = g;
  uintptr_t args[LAMBDA_BIND_MAX_ARGS] = {0};
#if LAMBDA_USE_MPROTECT
  long pagemask = ~(sysconf(_SC_PAGESIZE) - 1);
#endif

  for (int i = 0; i < n; i++)
    args[i] = va_arg(_args, uintptr_t);

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)g & pagemask), LAMBDA_BIND_SIZE(0,n,0), PROT_READ | PROT_WRITE)) return NULL;
#endif

  {
#ifdef ldr
    uintptr_t *d = (uintptr_t *)((uint32_t *)p + n + 2);
#endif
    for (int i = 0; i < n; i++) {
#ifdef ldr
      ldr(p, start + i, (uintptr_t)d - (uintptr_t)p);
      *d++ = args[i];
#else
      movi(p, start + i, args[i]);
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

  __builtin___clear_cache(g, (char *)g + LAMBDA_BIND_SIZE(0,n,0));

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)g & pagemask), LAMBDA_BIND_SIZE(0,n,0), PROT_READ | PROT_EXEC)) return NULL;
#endif

  return (fn_t)g;
}

fn_t lambda_compose(void *h, fn_t f, fn_t g) {
  void *p = h;

#if LAMBDA_USE_MPROTECT
  long pagemask = ~(sysconf(_SC_PAGESIZE) - 1);
  if (mprotect((void *)((uintptr_t)h & pagemask), LAMBDA_COMPOSE_SIZE, PROT_READ | PROT_WRITE)) return NULL;
#endif

#ifdef __aarch64__
  {
    uintptr_t *d = (uintptr_t *)((uint32_t *)p + 6);
    ldr(p, 16, (uintptr_t)d - (uintptr_t)p);
    *d++ = (uintptr_t)g;
    ldr(p, 17, (uintptr_t)d - (uintptr_t)p);
    *d = (uintptr_t)f;
    memcpy(p, (uint32_t []){
        0xA9BF47FE, // stp x30, x17, [sp, #-16]!
        0xD63F0200, // blr x16
        0xA8C147FE, // ldp x30, x17, [sp], #16
        0xD61F0220, // br  x17
      }, 16);
  }
#elif defined(__x86_64__)
  memcpy(p, (uchar []){
      0x48, 0xB8, u64((uintptr_t)g), // movabs rax, g
      0xFF, 0xD0,                    // call rax
      0x48, 0x89, 0xC7,              // mov rdi, rax
      0x48, 0xB8, u64((uintptr_t)f), // movabs rax, f
      0xFF, 0xE0                     // jmp rax
    }, 27);
#endif

  __builtin___clear_cache(h, (char *)h + LAMBDA_COMPOSE_SIZE);

#if LAMBDA_USE_MPROTECT
  if (mprotect((void *)((uintptr_t)h & pagemask), LAMBDA_COMPOSE_SIZE, PROT_READ | PROT_EXEC)) return NULL;
#endif

  return (fn_t)h;
}

#endif
