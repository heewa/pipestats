
SOURCES=pipestats.c units.c time_estimate.c
HEADERS=units.h time_estimate.h

ifeq ($(DEBUG), )
    CFLAGS=-Wall -O3
else
    CFLAGS=-Wall -O0 -g -ggdb -DDEBUG=1
endif
LDFLAGS=

CC=gcc

BUILD_DIR=build
OBJECTS=$(SOURCES:%.c=$(BUILD_DIR)/%.o)

all: pipestats misc

misc: generate_pattern sequential_bytes

pipestats: $(OBJECTS)
	$(CC) $(LDFLAGS) $(XFLAGS) $(OBJECTS) -o $@

generate_pattern: misc/generate_pattern.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

sequential_bytes: misc/sequential_bytes.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

# All objects depend on all headers, cuz hard to figure out dependency.
$(OBJECTS): $(BUILD_DIR)/%.o: %.c $(HEADERS) $(BUILD_DIR)
	$(CC) $(CFLAGS) $(XFLAGS) -c $< -o $@

$(BUILD_DIR):
	/bin/mkdir -v $(BUILD_DIR)

clean:
	rm -f pipestats generate_pattern sequential_bytes $(OBJECTS)
