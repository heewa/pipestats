
ifeq ($(DEBUG), )
    CFLAGS=-Wall -O3
else
    CFLAGS=-Wall -O0 -g -ggdb -DDEBUG=1
endif
LDFLAGS=

all: pipestats

pipestats: pipestats.c
	gcc $(CFLAGS) $(LDFLAGS) pipestats.c -o $@

clean:
	rm -f pipestats
