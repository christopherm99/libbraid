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
  braidexit(b);
}

void bar(void) {
  int i;
  for (i = 0; i < 5; i++) {
    printf("bar %d\n", i);
    braidyield(b);
  }
  braidexit(b);
}

int main(void) {
  b = braidinit();
  braidadd(b, foo, 1024);
  braidadd(b, bar, 1024);
  braidlaunch(b);
}

