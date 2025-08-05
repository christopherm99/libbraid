include config.mk

SRCS := $(wildcard src/*.c) $(wildcard src/io/*.c) $(wildcard src/io/$(BACKEND)/*.c)
OBJS := $(SRCS:src/%.c=build/%.o)
$(shell mkdir -p build)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -g -Iinclude \
          -Wno-strict-prototypes -Wno-incompatible-pointer-types -Wno-strict-aliasing
MACHINE := $(shell uname -m)

.PHONY: all clean examples

all: build/libbraid.a examples

build/libbraid.a: $(OBJS) build/ctxswap.$(MACHINE).o
	$(AR) rcs $@ $^

$(OBJS): build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/ctxswap.$(MACHINE).o: src/ctxswap.$(MACHINE).S
	$(AS) -c -o $@ $<

examples: build/libbraid.a
	$(MAKE) -C examples

install: build/libbraid.a
	install -d $(PREFIX)/lib
	install -m 644 build/libbraid.a $(PREFIX)/lib/

	install -d $(PREFIX)/include/braid
	install -m 644 include/*.h $(PREFIX)/include/
	install -m 644 include/braid/*.h $(PREFIX)/include/braid/

clean:
	rm -rf build/*
	$(MAKE) -C examples clean

