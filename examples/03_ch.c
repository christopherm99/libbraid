/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the use of channels in braids.
 */

#include <stdio.h>

#include <braid.h>
#include <braid/io.h>
#include <braid/ch.h>

int goal = 100;

void prime(braid_t b, ch_t c) {
  ch_t nc;
  int i, p;

  if ((p = chrecv(b, c)) > goal) braidstop(b);

  printf("%d\n", p);
  braidadd(b, prime, 65536, "prime", CORD_NORMAL, 1, nc = chcreate());

  for (;;) if ((i = chrecv(b, c)) % p) chsend(b, nc, i);
}

void start(braid_t b, ch_t c) {
  for (int i = 2;; i++) chsend(b, c, i);
}

int main(void) {
  braid_t b = braidinit();
  ch_t c = chcreate();

  braidadd(b, iovisor, 65536, "iovisor", CORD_SYSTEM, 0);
  braidadd(b, prime, 65536, "prime", CORD_NORMAL, 1, c);
  braidadd(b, start, 65536, "start", CORD_NORMAL, 1, c);
  braidstart(b);

  return 0;
}
