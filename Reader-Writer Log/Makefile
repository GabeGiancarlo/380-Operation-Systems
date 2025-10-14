CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
TARGET = rw_log
SOURCES = rw_main.c rwlog.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) log.csv

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

test: $(TARGET)
	./$(TARGET) --capacity 100 --readers 2 --writers 2 --writer-batch 1 --seconds 5 --rd-us 1000 --wr-us 1000 --dump

.PHONY: test
