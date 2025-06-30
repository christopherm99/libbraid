CFLAGS := -Wall -Wextra -Werror -pedantic -ansi -ggdb
MACHINE := $(shell uname -m)

.PHONY: all clean

all: libbraid.a

libbraid.a: ctx.o braid.o swapctx.$(MACHINE).o
	$(AR) rcs $@ $^

ctx.o: ctx.c
	$(CC) $(CFLAGS) -c $<

braid.o: braid.c
	$(CC) $(CFLAGS) -c $<

swapctx.$(MACHINE).o: swapctx.$(MACHINE).S
	$(AS) -c -o $@ $<

test: libbraid.a test.c
	$(CC) $(CFLAGS) -o test test.c -L. -lbraid

testbraid: libbraid.a testbraid.c
	$(CC) $(CFLAGS) -o testbraid testbraid.c -L. -lbraid

clean:
	rm -f *.o libbraid.a test testbraid

