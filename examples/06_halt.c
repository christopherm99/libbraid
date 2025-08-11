/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates using cordhalt.
 */

#include <stdio.h>
#include <braid.h>

void bar(braid_t b, cord_t c) {
  cordhalt(b, c);
  puts("halted!");
}

void foo(braid_t b) {
  braidadd(b, bar, 65536, "bar", CORD_NORMAL, 1, (usize)braidcurr(b));
  braidyield(b);
  puts("i will never print!");
}

int main(void) {
  braid_t b = braidinit();
  braidadd(b, foo, 65536, "foo", CORD_NORMAL, 0);
  braidstart(b);
  return 0;
}

