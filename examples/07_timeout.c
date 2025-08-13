/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the timeout api.
 */
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wunused-result"

#include <braid.h>
#include <braid/ck.h>
#include <braid/io.h>
#include <braid/fd.h>

#include <err.h>
#include <unistd.h>

void foo(braid_t b) {
  char buf[100];
  for (;;) {
    int rc;
    if ((rc = cktimeout(b, (usize (*)(usize))fdread, 65536, 1, 4, b, 0, buf, 100))) write(1, buf, rc);
    else return;
  }
}

int main(void) {
  braid_t b = braidinit();
  braidadd(b, iovisor, 65536, "iovisor", CORD_SYSTEM, 0);
  braidadd(b, foo, 65536, "foo", CORD_NORMAL, 0);
  braidstart(b);
  return 0;
}

