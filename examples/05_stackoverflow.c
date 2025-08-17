/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates a stack overflow in a cord.
 */

#include <braid.h>

#pragma GCC diagnostic ignored "-Winfinite-recursion"
int fn(void) {
  volatile char buf[1024];
  return fn() + buf[0];
}

int main(void) {
  braid_t b = braidinit();
  braidadd(b, (void (*)(void))fn, 8196, "fn", CORD_NORMAL, 0);
  braidstart(b);
  return 0;
}

