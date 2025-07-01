/* Copyright (c) 2025 Christopher Milan; see LICENSE
 *
 * This file demonstrates non-blocking IO in braids. 
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "braid.h"
#include "fd.h"

static int pipefd[2];

void producer(braid_t b) {
  int i;
  const char *msg = "Hello!";
  for (i = 0; i < 5; i++) {
    if (fdwrite(b, pipefd[1], msg, strlen(msg)) < 0)
      err(EX_OSERR, "producer: fdwrite");
    printf("sent: %s\n", msg);
  }
  close(pipefd[1]);
}

void consumer(braid_t b) {
  char buf[32];
  while (1) {
    int rc;
    if ((rc = fdread(b, pipefd[0], buf, sizeof(buf) - 1)) < 0)
      err(EX_OSERR, "consumer: fdread");
    else if (rc == 0) break;
    buf[rc] = '\0';
    printf("received: %s\n", buf);
  }
  printf("consumer: EOF reached, exiting\n");
}

int main(void) {
  braid_t b = braidinit();

  if (pipe(pipefd) < 0) err(EX_OSERR, "pipe");
  fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL)|O_NONBLOCK);
  fcntl(pipefd[1], F_SETFL, fcntl(pipefd[1], F_GETFL)|O_NONBLOCK);

  braidadd(b, fdvisor, 65536, "fdvisor", CORD_SYSTEM);
  braidadd(b, producer, 65536, "producer", CORD_NORMAL);
  braidadd(b, consumer, 65536, "consumer", CORD_NORMAL);

  braidstart(b);
  return 0;
}

