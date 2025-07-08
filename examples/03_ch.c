/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the use of channels in braids.
 */

#include <stdio.h>
#include <stdlib.h>

#include <braid.h>
#include <braid/ch.h>

int goal = 100;

void prime(braid_t b, ch_t c) {
  ch_t nc;
  int i, p;

  if ((p = chrecv(b, c)) > goal) exit(0);

  printf("%d\n", p);
  braidadd(b, prime, 65536, "prime", CORD_NORMAL, nc = chcreate(b));

  for (;;) if ((i = chrecv(b, c)) % p) chsend(b, nc, i);
}

void start(braid_t b, ch_t c) {
  for (int i = 2;; i++) chsend(b, c, i);
}

int main(void) {
  braid_t b = braidinit();
  ch_t c = chcreate(b);

  braidadd(b, chvisor, 65536, "chvisor", CORD_SYSTEM, 0);
  braidadd(b, prime, 65536, "prime", CORD_NORMAL, c);
  braidadd(b, start, 65536, "start", CORD_NORMAL, c);
  braidstart(b);

  return 0;
}
