/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates a stack overflow in a cord.
 */

#include <braid.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
void fn(void) { fn(); }
#pragma GCC diagnostic pop

int main(void) {
  braid_t b = braidinit();
  braidadd(b, fn, 8196, "fn", CORD_NORMAL, 0);
  braidstart(b);
  return 0;
}

