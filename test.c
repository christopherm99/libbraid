#include <stdio.h>

#include "braid.h"

static cord_t x, y;

void fn(void) {
  printf("hello\n");
  cordswitch(x);
}

int main(void) {
  x = cordactive();
  y = cordcreate(fn, 0x1000);

  printf("switching to y\n");
  cordswitch(y);
  printf("back to x\n");
}

