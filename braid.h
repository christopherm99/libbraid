#ifndef _BRAID_H
#define _BRAID_H

#include "ctx.h"

typedef struct cord Cord;
struct cord {
  ctx_t  ctx;
  void (*entry)();
  Cord  *next;
  Cord  *prev;
  char   name[16];
};

typedef struct {
  Cord *head;
  Cord *tail;
} CordList;

typedef struct {
  ctx_t    sched;
  Cord    *running;
  CordList cords;
} Braid;

Braid *braidinit(void);
void braidadd(Braid *b, void (*f)(), usize stacksize);
void braidlaunch(Braid *b);
void braidyield(Braid *b);
void braidexit(Braid *b);

#endif

