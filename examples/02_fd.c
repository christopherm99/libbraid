/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates the use of non-blocking IO in braids.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <braid.h>
#include <braid/io.h>
#include <braid/fd.h>

void producer(braid_t b, usize fd) {
  int i;
  const char *msg = "Hello!";
  for (i = 0; i < 5; i++) {
    if (fdwrite(b, fd, msg, strlen(msg)) < 0)
      err(EX_OSERR, "producer: fdwrite");
    printf("sent: %s\n", msg);
  }
  close(fd);
}

void consumer(braid_t b, usize fd) {
  char buf[32];
  while (1) {
    int rc;
    if ((rc = fdread(b, fd, buf, sizeof(buf) - 1)) < 0)
      err(EX_OSERR, "consumer: fdread");
    else if (rc == 0) break;
    buf[rc] = '\0';
    printf("received: %s\n", buf);
  }
  printf("consumer: EOF reached, exiting\n");
}

int main(void) {
  int pipefd[2];
  braid_t b = braidinit();

  if (pipe(pipefd) < 0) err(EX_OSERR, "pipe");

  braidadd(b, iovisor, 65536, "iovisor", CORD_SYSTEM, 0);
  braidadd(b, producer, 65536, "producer", CORD_NORMAL, pipefd[1]);
  braidadd(b, consumer, 65536, "consumer", CORD_NORMAL, pipefd[0]);

  braidstart(b);
  return 0;
}

