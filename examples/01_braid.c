/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the braid API.
 */

#include <stdio.h>

#include "braid.h"

void foo(braid_t b) {
  int i;
  for (i = 0; i < 5; i++) {
    printf("foo %d\n", i);
    braidyield(b);
  }
  printf("foo done\n");
}

void bar(braid_t b) {
  int i;
  for (i = 0; i < 5; i++) {
    printf("bar %d\n", i);
    braidyield(b);
  }
}

int main(void) {
  braid_t b = braidinit();
  braidadd(b, foo, 65536, "foo", CORD_NORMAL);
  braidadd(b, bar, 65536, "bar", CORD_NORMAL);
  braidstart(b);
  return 0;
}

