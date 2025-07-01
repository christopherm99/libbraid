SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c, build/%.o, $(SRCS))
$(shell mkdir -p build)

CFLAGS := -Wall -Wextra -Werror -pedantic -ansi -g -Iinclude
MACHINE := $(shell uname -m)
PREFIX ?= /usr/local

.PHONY: all clean examples

all: build/libbraid.a examples

build/libbraid.a: $(OBJS) build/swapctx.$(MACHINE).o
	$(AR) rcs $@ $^

$(OBJS): build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build/swapctx.$(MACHINE).o: src/swapctx.$(MACHINE).S
	$(AS) -c -o $@ $<

examples: build/libbraid.a
	$(MAKE) -C examples

install: build/libbraid.a
	install -d $(PREFIX)/lib
	install -m 644 build/libbraid.a $(PREFIX)/lib/

	install -d $(PREFIX)/include/braid
	install -m 644 include/*.h $(PREFIX)/include/braid/

clean:
	rm -rf build/*
	$(MAKE) -C examples clean

