/* pool.h - v0.1
 * A memory pool allocator.
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

#ifdef POOL_IMPLEMENTATION
#ifndef POOL_DEFINITIONS
#define POOL_DEFINITIONS
#define POOL_USE_MPROTECT 0
// set this to the mprotect value to restore to (usually PROT_READ|PROT_EXEC)
#define POOL_MPROTECT_RESTORE 0
#endif
#endif

#ifndef _POOL_H
#define _POOL_H

typedef struct pool * pool_t;

// configures a region of memory to work as a memory pool
pool_t pool_new(void *mem, int len, int chunk_sz);
// returns a pointer to a chunk of memory of size POOL_CHUNK_SIZE
void *pool_alloc(pool_t p) __attribute__((malloc));
// frees a chunk of memory previously allocated by pool_alloc
void pool_free(pool_t p, void *ptr);

#endif

#ifdef POOL_IMPLEMENTATION
#include <stdint.h>
#include <unistd.h>
typedef uintptr_t usize;
typedef unsigned char uchar;
struct pool {
  int len, chunk_sz;
  uchar *wilderness;
  void *free;
  uchar data[];
};

pool_t pool_new(void *mem, int len, int chunk_sz) {
  pool_t p = mem;
  p->len = len;
  p->chunk_sz = chunk_sz;
  p->wilderness = (uchar *)&p->data;
  p->free = 0;
  return p;
}

void *pool_alloc(pool_t p) {
  void *ret;
#if POOL_USE_MPROTECT
  if (mprotect(p, sizeof(struct pool), PROT_READ | PROT_WRITE)) return 0;
#endif

  if (p->free) {
    ret = p->free;
    p->free = *(void **)p->free;
    return ret;
  }

  if ((usize)p->wilderness + p->chunk_sz > (usize)p + p->len) return 0;

  ret = p->wilderness;
  p->wilderness = p->wilderness + p->chunk_sz;
#if POOL_USE_MPROTECT
  if (mprotect(p, sizeof(struct pool), POOL_MPROTECT_RESTORE)) return 0;
#endif
  return ret;
}

void pool_free(pool_t p, void *ptr) {
#if POOL_USE_MPROTECT
  mprotect(p, sizeof(struct pool), PROT_READ | PROT_WRITE);
#endif
  if ((uchar *)ptr + p->chunk_sz == p->wilderness) p->wilderness -= p->chunk_sz;
  else {
#if POOL_USE_MPROTECT
    uintptr_t ptr_page = (uintptr_t)ptr & ~(sysconf(_SC_PAGESIZE) - 1);
    mprotect((void *)ptr_page, (uintptr_t)ptr - ptr_page + sizeof(struct pool), PROT_READ | PROT_WRITE);
#endif
    *(void **)ptr = p->free;
    p->free = ptr;
#if POOL_USE_MPROTECT
    mprotect((void *)ptr_page, (uintptr_t)ptr - ptr_page + sizeof(struct pool), POOL_MPROTECT_RESTORE);
#endif
  }
#if POOL_USE_MPROTECT
  mprotect(p, sizeof(struct pool), POOL_MPROTECT_RESTORE);
#endif
}

#endif
