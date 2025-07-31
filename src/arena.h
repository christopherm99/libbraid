/* arena.h - v0.2
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
#include <stdlib.h>
#define ARENA_FREE(x)      free(x)
#define ARENA_MALLOC(x)    malloc(x)
#define ARENA_REALLOC(p,x) realloc(p,x)
#endif
#endif
#ifndef _ARENA_H
#define _ARENA_H
#include <stddef.h>

typedef struct arena * arena_t;

arena_t arena_create(void);
void    arena_destroy(arena_t a);
void   *arena_malloc(arena_t a, size_t n);
void   *arena_zalloc(arena_t a, size_t n);
void   *arena_realloc(arena_t a, void *p, size_t n);
void    arena_free(arena_t a, void *p);

char *arena_strdup(arena_t a, const char *s);

#endif
#ifdef ARENA_IMPLEMENTATION
#include <string.h>

struct arena {
  struct block {
    struct block *prev, *next;
  } *head;
};

arena_t arena_create(void) {
  arena_t a = (arena_t)ARENA_MALLOC(sizeof(struct arena));
  a->head = 0;
  return a;
}

void arena_destroy(arena_t a) {
  struct block *b;
  while ((b = a->head)) {
    a->head = b->next;
    ARENA_FREE(b);
  }
  ARENA_FREE(a);
}

void *arena_malloc(arena_t a, size_t n) {
  struct block *b;
  if (!(b = ARENA_MALLOC(sizeof(struct block) + n))) return 0;
  b->prev = 0;
  b->next = a->head;
  a->head = b;
  return b + 1;
}

void *arena_zalloc(arena_t a, size_t n) {
  void *ret = arena_malloc(a, n);
  if (ret) memset(ret, 0, n);
  return ret;
}

void *arena_realloc(arena_t a, void *p, size_t n) {
  struct block *b = (struct block *)p - 1, *nb;
  if (!p) return arena_malloc(a, n);
  if (!(nb = ARENA_REALLOC(b, sizeof(sizeof(struct block) + n)))) return 0;
  if (b != nb) {
    if (nb->prev) nb->prev = nb;
    else a->head = nb;
    if (nb->next) nb->next->prev = nb;
  }
  return nb + 1;
}

void arena_free(arena_t a, void *p) {
  struct block *b = (struct block *)p - 1;
  if (b->prev) b->prev->next = b->next;
  else a->head = b->next;
  if (b->next) b->next->prev = b->prev;
  ARENA_FREE(b);
}

char *arena_strdup(arena_t a, const char *s) {
  char *d;
  if (s == NULL) return NULL;
  if ((d = arena_malloc(a, strlen(s) + 1))) strcpy(d, s);
  return d;
}

#endif
