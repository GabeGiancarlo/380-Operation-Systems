# Makefile for Virtual Memory Manager
# Compiles vmmgr.c with proper flags and dependencies

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
TARGET = vmmgr
SOURCES = vmmgr.c
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

# Run the program with addresses.txt
test: $(TARGET)
	@echo "Running virtual memory manager with addresses.txt..."
	@./$(TARGET) addresses.txt
	@echo ""
	@echo "Test completed."

# Help target
help:
	@echo "Virtual Memory Manager Makefile"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the vmmgr executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  test     - Run the program with addresses.txt"
	@echo "  help     - Show this help message"

