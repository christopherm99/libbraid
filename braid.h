#ifndef _BRAID_H
#define _BRAID_H

#include "types.h"

#define CORD_NORMAL 0x00
#define CORD_SYSTEM 0x01

typedef struct cord Cord;
struct cord {
  ctx_t  ctx;
  void (*entry)();
  Cord  *next;
  Cord  *prev;
  uint   flags;
  char   name[16];
};

typedef struct {
  Cord *head;
  Cord *tail;
  uint  count;
} CordList;

typedef struct {
  ctx_t    sched;
  Cord    *running;
  CordList cords;
} Braid;

Braid *braidinit(void);
void braidadd(Braid *b, void (*f)(), usize stacksize, uint flags);
void braidlaunch(Braid *b);
void braidyield(Braid *b);
void braidexit(Braid *b);

#endif

