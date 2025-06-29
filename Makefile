CFLAGS := -Wall -Wextra -pedantic -std=c99
MACHINE := $(shell uname -m)

.PHONY: all clean

all: libbraid.a

libbraid.a: braid.o ctx_swap.$(MACHINE).o
	$(AR) rcs $@ $^

braid.o: braid.c
	$(CC) $(CFLAGS) -c $<

ctx_swap.$(MACHINE).o: ctx_swap.$(MACHINE).S
	$(AS) -c -o $@ $<

clean:
	rm -f *.o libbraid.a

