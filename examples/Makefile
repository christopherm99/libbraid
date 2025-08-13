SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
PROGS := $(SRCS:.c=)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -g -I../include -fsanitize=address
LDFLAGS := -L../build $(shell pkg-config --libs ../build/libbraid.pc) -fsanitize=address

.PHONY: all clean

all: $(PROGS)

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGS): %: %.o ../build/libbraid.a
	$(CC) $< $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(PROGS)
