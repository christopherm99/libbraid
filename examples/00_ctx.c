/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstates the ctx API. Understanding the ctx API is not
 * required to use the higher-level braid API.
 */

#include <stdio.h>

#include <braid/ctx.h>

static ctx_t x, y;

void fn(usize arg) {
  printf("hello %lu\n", arg);
  ctxswap(y, x);
}

int main(void) {
  x = ctxempty();
  y = ctxcreate(fn, 0x1000, 1337);

  printf("switching to y\n");
  ctxswap(x, y);
  printf("back to x\n");
  return 0;
}

