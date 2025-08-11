SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
PROGS := $(SRCS:.c=)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 \
          -Wno-strict-prototypes -g -I../include
LDFLAGS := -L../build $(shell pkg-config --libs ../build/libbraid.pc)

.PHONY: all clean

all: $(PROGS)

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGS): %: %.o ../build/libbraid.a
	$(CC) $< $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(PROGS)
