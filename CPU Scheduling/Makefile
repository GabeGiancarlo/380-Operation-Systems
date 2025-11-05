# Makefile for CPU Scheduling Simulator
# Compiles schedsim.c with proper flags and dependencies

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -O2 -g
LDFLAGS = -lpthread
TARGET = schedsim
SOURCES = schedsim.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test install help

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Run tests
test: $(TARGET)
	@echo "Testing FCFS scheduling..."
	./$(TARGET) -f -i test_processes.csv
	@echo ""
	@echo "Testing SJF scheduling..."
	./$(TARGET) -s -i test_processes.csv
	@echo ""
	@echo "Testing Round Robin scheduling..."
	./$(TARGET) -r -i test_processes.csv -q 4
	@echo ""
	@echo "Testing Priority scheduling..."
	./$(TARGET) -p -i test_processes.csv

# Help target
help:
	@echo "CPU Scheduling Simulator Makefile"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the schedsim executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  test     - Run all scheduling algorithm tests"
	@echo "  install  - Install schedsim to /usr/local/bin"
	@echo "  help     - Show this help message"

