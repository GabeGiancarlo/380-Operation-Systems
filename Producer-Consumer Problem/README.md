#Group Members
    Gabriel Giancarlo (2405449)
    Nate Carnnahan (2369448)

# Producer-Consumer Problem with Bounded Buffer

This project implements the classic Producer-Consumer synchronization problem using C with proper thread synchronization mechanisms.

## Overview

The Producer-Consumer problem demonstrates coordination between multiple threads that share a bounded buffer. Producers generate data items and insert them into the buffer, while consumers remove and process these items. The implementation ensures thread safety and prevents race conditions using mutexes and semaphores.

## Design Choices

### C Implementation (`prodcon.c`)
- **Synchronization Primitives**: Uses `pthread_mutex_t` for mutual exclusion and `sem_t` for counting semaphores
- **Thread Management**: POSIX threads (`pthread`) for cross-platform compatibility
- **Memory Management**: Manual memory allocation with proper cleanup
- **Error Handling**: Traditional C error handling with `perror()` and `exit()`

### Synchronization Strategy
The implementation uses the classic three-primitive approach:
1. **Mutex**: Ensures mutual exclusion in critical sections
2. **Empty Semaphore**: Tracks available buffer slots (initialized to buffer size)
3. **Full Semaphore**: Tracks occupied buffer slots (initialized to 0)

### Data Integrity
- **Checksum Validation**: Each `BUFFER_ITEM` contains a checksum for data integrity verification
- **Error Detection**: Consumers verify checksums and report corruption
- **Graceful Termination**: Clean shutdown with proper thread joining

## Files

- `buffer.h` - Header defining `BUFFER_ITEM` structure and buffer size
- `prodcon.c` - C implementation using pthread synchronization
- `README.md` - This documentation file

## Compilation Instructions

```bash
gcc -Wall -Werror -std=c99 -pthread -o prodcon prodcon.c
```

**Note**: Requires `-pthread` flag for thread support.

## Usage

```bash
./prodcon <delay> <#producers> <#consumers>
```

### Arguments
- `delay`: Number of seconds to run before terminating (positive integer)
- `#producers`: Number of producer threads (positive integer)
- `#consumers`: Number of consumer threads (positive integer)

### Examples
```bash
# Run for 5 seconds with 2 producers and 3 consumers
./prodcon 5 2 3

# Run for 10 seconds with 1 producer and 1 consumer
./prodcon 10 1 1
```

## Expected Output

The program will display output similar to:

```
Starting Producer-Consumer with 2 producers, 3 consumers, delay 5 seconds
Producer 0 starting
Producer 1 starting
Consumer 0 starting
Consumer 1 starting
Consumer 2 starting
Sleeping for 5 seconds...
Producer 0: inserted item with checksum 245
Consumer 0: consumed item with checksum 245
Producer 1: inserted item with checksum 189
Consumer 1: consumed item with checksum 189
...
Stopping all threads...
Producer 0 terminating
Producer 1 terminating
Consumer 0 terminating
Consumer 1 terminating
Consumer 2 terminating
Program completed successfully
```

## Key Features

### Thread Safety
- **Mutex Protection**: Critical sections are protected by mutex locks
- **Semaphore Coordination**: Proper signaling between producers and consumers
- **Atomic Operations**: Thread-safe flag management for clean termination

### Data Integrity
- **Checksum Validation**: Each data item includes a checksum for corruption detection
- **Error Reporting**: Detailed error messages for debugging
- **Graceful Failure**: Program exits on data corruption detection

### Resource Management
- **RAII (C++)**: Automatic resource cleanup with destructors
- **Manual Cleanup (C)**: Explicit resource deallocation
- **Memory Safety**: Proper allocation and deallocation patterns

## Testing

Both implementations include comprehensive error handling and validation:

1. **Argument Validation**: Checks for positive integers and correct argument count
2. **Thread Safety**: Multiple producers and consumers can run concurrently
3. **Data Integrity**: Checksum validation ensures data hasn't been corrupted
4. **Resource Cleanup**: Proper cleanup of synchronization primitives

## Performance Considerations

- **Buffer Size**: Fixed at 10 slots (configurable via `BUFFER_SIZE`)
- **Thread Overhead**: Minimal delay between operations to prevent system overload
- **Memory Usage**: Efficient circular buffer implementation
- **Synchronization**: Minimal lock contention with proper semaphore usage

## Error Handling

The implementations handle various error conditions:

- **Invalid Arguments**: Clear error messages for incorrect usage
- **Thread Creation Failures**: Proper error reporting and cleanup
- **Synchronization Errors**: Graceful handling of mutex/semaphore failures
- **Data Corruption**: Immediate termination on checksum mismatch

## Platform Compatibility

- **C Implementation**: Compatible with POSIX systems (Linux, macOS, Unix)
- **Threading**: Uses standard pthread library with cancellation support
- **Semaphores**: Uses POSIX named semaphores for robust synchronization
- **Compilation**: Tested with GCC and Clang compilers
