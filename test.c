#include <stdio.h>

#include "ctx.h"

static ctx_t x, y;

void fn(void) {
  printf("hello\n");
  swapctx(y, x);
}

int main(void) {
  x = newctx();
  y = createctx(fn, 0x1000);

  printf("switching to y\n");
  swapctx(x, y);
  printf("back to x\n");
}

