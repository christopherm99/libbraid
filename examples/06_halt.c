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

void foo(braid_t b, usize arg) {
  braidadd(b, bar, 65536, "bar", CORD_NORMAL, (usize)braidcurr(b));
  braidyield(b);
  puts("i will never print!");
  (void)arg;
}

int main(void) {
  braid_t b = braidinit();
  braidadd(b, foo, 65536, "foo", CORD_NORMAL, 1337);
  braidstart(b);
  return 0;
}

