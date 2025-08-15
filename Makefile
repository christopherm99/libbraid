include config.mk

SRCS := $(wildcard src/*.c) $(wildcard src/io/*.c) $(wildcard src/io/$(BACKEND)/*.c)
OBJS := $(SRCS:src/%.c=build/%.o)
$(shell mkdir -p build)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O2 -g -Iinclude
MACHINE := $(shell uname -m)

.PHONY: all clean examples install

all: build/libbraid.a build/libbraid.pc examples

build/libbraid.a: $(OBJS) build/ctxswap.$(MACHINE).o
	$(AR) rcs $@ $^

$(OBJS): build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/ctxswap.$(MACHINE).o: src/ctxswap.$(MACHINE).S
	$(AS) -c -o $@ $<

build/libbraid.pc: libbraid.pc
	sed -e "s|@PREFIX@|$(PREFIX)|" \
			-e "s|@PRIVATE_LIBS@|$(PRIVATE_LIBS)|" \
			$< > $@

examples: build/libbraid.a build/libbraid.pc
	$(MAKE) -C examples

install: build/libbraid.a build/libbraid.pc
	install -d $(PREFIX)/lib
	install -m 644 build/libbraid.a $(PREFIX)/lib/

	install -d $(PREFIX)/include/braid
	install -m 644 include/*.h $(PREFIX)/include/
	install -m 644 include/braid/*.h $(PREFIX)/include/braid/

	install -d $(PREFIX)/lib/pkgconfig
	install -m 644 build/libbraid.pc $(PREFIX)/lib/pkgconfig/libbraid.pc

clean:
	rm -rf build/*
	$(MAKE) -C examples clean

