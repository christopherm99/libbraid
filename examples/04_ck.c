/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the use of clocks in braids.
 */

#include <stdio.h>

#include <braid.h>
#include <braid/io.h>
#include <braid/ck.h>

void sleeper(braid_t b, uint ns) {
  cknsleep(b, ns);
  printf("slept for %u ns\n", ns);
}

int main(void) {
  braid_t b = braidinit();

  braidadd(b, iovisor, 65536, "iovisor", CORD_SYSTEM, 0);
  braidadd(b, sleeper, 65536, "sleeper1", CORD_NORMAL, 2, b, 1000000000); // 1 second
  braidadd(b, sleeper, 65536, "sleeper2", CORD_NORMAL, 2, b, 500000000); // 0.5 seconds
  braidadd(b, sleeper, 65536, "sleeper3", CORD_NORMAL, 2, b, 250000000); // 0.25 seconds

  braidstart(b);

  return 0;
}


