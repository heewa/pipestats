
ifeq ($(DEBUG), )
    CFLAGS=-Wall -O3
else
    CFLAGS=-Wall -O0 -g -ggdb -DDEBUG=1
endif
LDFLAGS=

all: pipestats misc

misc: generate_pattern sequential_bytes

pipestats: pipestats.c
	gcc $(CFLAGS) $(LDFLAGS) $(XFLAGS) pipestats.c -o $@

generate_pattern: misc/generate_pattern.c
	gcc $(CFLAGS) $(LDFLAGS) $< -o $@

sequential_bytes: misc/sequential_bytes.c
	gcc $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f pipestats generate_pattern
