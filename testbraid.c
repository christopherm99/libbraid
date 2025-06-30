#include <stdio.h>

#include "braid.h"

static Braid *b;

void foo(void) {
  int i;
  for (i = 0; i < 5; i++) {
    printf("foo %d\n", i);
    braidyield(b);
  }
  printf("foo done\n");
}

void bar(void) {
  int i;
  for (i = 0; i < 5; i++) {
    printf("bar %d\n", i);
    braidyield(b);
  }
}

int main(void) {
  b = braidinit();
  braidadd(b, foo, 65536, CORD_NORMAL);
  braidadd(b, bar, 65536, CORD_NORMAL);
  braidlaunch(b);
  return 0;
}

