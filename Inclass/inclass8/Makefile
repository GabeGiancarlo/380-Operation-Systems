CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: addresses

addresses: addresses.c
	$(CC) $(CFLAGS) -o addresses addresses.c

clean:
	rm -f addresses addresses.o

.PHONY: all clean

