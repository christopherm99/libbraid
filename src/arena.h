/* arena.h - v0.1
 * An arena allocator.
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

#ifdef ARENA_IMPLEMENTATION
#ifndef ARENA_DEFINITIONS
#define ARENA_DEFINITIONS
#include <unistd.h>
#include <sys/mman.h>
#define ARENA_REGION_SIZE sysconf(_SC_PAGESIZE)
#define ARENA_REGION_ALLOC(r,sz) (r = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0)) == MAP_FAILED
#define ARENA_REGION_FREE(r,sz) munmap(r, sz)

#endif
#endif
#ifndef _ARENA_H
#define _ARENA_H
#include <stddef.h>

typedef struct arena * arena_t;

arena_t arena_create(void);
void    arena_destroy(arena_t a);
void   *arena_malloc(arena_t a, size_t size);
void   *arena_zalloc(arena_t a, size_t size);
char   *arena_strdup(arena_t a, const char *s);


#endif
#ifdef ARENA_IMPLEMENTATION
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct region {
  struct region *next;
  size_t cnt, cap;
  uintptr_t data[];
}* region_t;

struct arena {
  region_t head, tail;
};

static inline struct region *create_region(size_t sz) {
  region_t r;
  if (ARENA_REGION_ALLOC(r, sz)) return 0;
  memset(r, 0, sizeof(*r));
  r->cap = (sz - sizeof(*r)) / sizeof(uintptr_t);
  return r;
}

arena_t arena_create(void) {
  return calloc(1, sizeof(struct arena));
}

void arena_destroy(arena_t a) {
  for (region_t r = a->head; r; r = r->next) ARENA_REGION_FREE(r, ARENA_REGION_SIZE);
  free(a);
}

void *arena_malloc(arena_t a, size_t size) {
  void *ret;
  size_t sz = (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
  region_t r = a->head;
  if (size > (size_t)ARENA_REGION_SIZE) {
    if (!(r = create_region(size + sizeof(*r)))) return 0;
    r->next = a->head;
    if (!a->tail) a->tail = r->next;
  } else if (!r || a->head->cnt + sz > a->head->cap) {
    if (!(r = create_region(ARENA_REGION_SIZE))) return 0;
    r->next = a->head;
    if (!a->tail) a->tail = a->head;
  } else for (;r->next && r->next->cnt + sz > a->head->cap; r = r->next);
  ret = &r->data[r->cnt];
  r->cnt += sz;
  return ret;
}

void *arena_zalloc(arena_t a, size_t size) { return memset(arena_malloc(a, size), 0, size); }

char *arena_strdup(arena_t a, const char *s) {
  char *d;
  if (s == NULL) return NULL;
  if ((d = arena_malloc(a, strlen(s) + 1))) strcpy(d, s);
  return d;
}

#endif
