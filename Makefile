CFLAGS := -Wall -Wextra -Werror -pedantic -ansi -g
MACHINE := $(shell uname -m)

.PHONY: all clean examples

all: libbraid.a

libbraid.a: ctx.o braid.o swapctx.$(MACHINE).o
	$(AR) rcs $@ $^

ctx.o: ctx.c
	$(CC) $(CFLAGS) -c $<

braid.o: braid.c
	$(CC) $(CFLAGS) -c $<

swapctx.$(MACHINE).o: swapctx.$(MACHINE).S
	$(AS) -c -o $@ $<

examples: libbraid.a
	$(MAKE) -C examples

clean:
	rm -f *.o libbraid.a
	$(MAKE) -C examples clean

