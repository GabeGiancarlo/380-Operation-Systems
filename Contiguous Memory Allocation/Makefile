# Makefile for Contiguous Memory Allocator
# Compiles allocator.c with proper flags and dependencies

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
TARGET = allocator
SOURCES = allocator.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test help

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Run basic tests
test: $(TARGET)
	@echo "Running basic allocation test..."
	@echo "RQ P1 40000 F" > test_input.txt
	@echo "RQ P2 50000 B" >> test_input.txt
	@echo "RL P1" >> test_input.txt
	@echo "RQ P3 60000 W" >> test_input.txt
	@echo "STAT" >> test_input.txt
	@echo "C" >> test_input.txt
	@echo "STAT -v" >> test_input.txt
	@echo "X" >> test_input.txt
	@echo "1048576" | ./$(TARGET) < test_input.txt || true
	@rm -f test_input.txt
	@echo ""
	@echo "Test completed. Check output above."

# Help target
help:
	@echo "Contiguous Memory Allocator Makefile"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the allocator executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  test     - Run basic allocation tests"
	@echo "  help     - Show this help message"

