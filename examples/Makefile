SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
PROGS := $(SRCS:.c=)

CFLAGS := -Wall -Wextra -pedantic -std=c99 -I../include -Wno-incompatible-pointer-types
LDFLAGS := -L../build -lbraid

.PHONY: all clean

all: $(PROGS)

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGS): %: %.o
	$(CC) $< $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(PROGS)
