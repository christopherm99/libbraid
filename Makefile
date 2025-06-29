CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -ggdb
MACHINE := $(shell uname -m)

.PHONY: all clean

all: libbraid.a

libbraid.a: braid.o ctx_swap.$(MACHINE).o
	$(AR) rcs $@ $^

braid.o: braid.c
	$(CC) $(CFLAGS) -c $<

ctx_swap.$(MACHINE).o: ctx_swap.$(MACHINE).S
	$(AS) -c -o $@ $<

test: libbraid.a test.c
	$(CC) $(CFLAGS) -o test test.c -L. -lbraid

clean:
	rm -f *.o libbraid.a test

