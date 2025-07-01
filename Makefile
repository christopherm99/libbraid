SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

CFLAGS := -Wall -Wextra -Werror -pedantic -ansi -g
MACHINE := $(shell uname -m)

.PHONY: all clean examples

all: libbraid.a

libbraid.a: $(OBJS) swapctx.$(MACHINE).o
	$(AR) rcs $@ $^

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $<

swapctx.$(MACHINE).o: swapctx.$(MACHINE).S
	$(AS) -c -o $@ $<

examples: libbraid.a
	$(MAKE) -C examples

clean:
	rm -f *.o libbraid.a
	$(MAKE) -C examples clean

