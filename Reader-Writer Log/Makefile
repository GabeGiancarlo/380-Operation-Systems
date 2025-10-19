CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -O2 -g
LDFLAGS = -lrt -lpthread
TARGET = rw_log
SOURCES = rw_main.c rwlog.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test install debug

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) log.csv

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

test: $(TARGET)
	./$(TARGET) --capacity 100 --readers 2 --writers 2 --writer-batch 1 --seconds 5 --rd-us 1000 --wr-us 1000 --dump

debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

# Performance test with more threads
perf-test: $(TARGET)
	./$(TARGET) --capacity 1024 --readers 8 --writers 4 --writer-batch 3 --seconds 10 --rd-us 1000 --wr-us 2000 --dump

# Stress test
stress-test: $(TARGET)
	./$(TARGET) --capacity 2048 --readers 16 --writers 8 --writer-batch 5 --seconds 30 --rd-us 500 --wr-us 1000 --dump

.PHONY: test debug perf-test stress-test
