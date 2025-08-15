/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the use of channels in braids.
 * NOTE: maybe run ulimit -n 65536 first.
 */
#pragma GCC diagnostic ignored "-Wunused-result"

#include <stdio.h>
#include <unistd.h>

#include <braid.h>
#include <braid/io.h>
#include <braid/ch.h>

int goal = 3;
int last = 0;

void prime(braid_t b, ch_t c) {
  ch_t nc;
  int i, p;

  if ((p = chrecv(b, c, 0)) > goal) {
    printf("%d\n", p);
    braidstop(b);
  }

  if (p > last + 100) {
    last = p;
    write(0, ".", 1);
  }

  braidadd(b, prime, 65536, "prime", CORD_NORMAL, 2, b, nc = chopen(b));

  for (;;) if ((i = chrecv(b, c, 0)) % p) chsend(b, nc, i);
}

void start(braid_t b, ch_t c) {
  for (int i = 2;; i++) chsend(b, c, i);
}

int main(void) {
  braid_t b = braidinit();
  ch_t c = chopen(b);

  braidadd(b, iovisor, 65536, "iovisor", CORD_SYSTEM, 0);
  braidadd(b, prime, 65536, "prime", CORD_NORMAL, 2, b, c);
  braidadd(b, start, 65536, "start", CORD_NORMAL, 2, b, c);
  braidstart(b);

  return 0;
}
