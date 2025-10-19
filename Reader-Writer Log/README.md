# Reader-Writer Log Synchronization

This implementation provides a thread-safe logging system with reader-writer synchronization using POSIX threads and shared memory.

## Features

- **Writer-preference synchronization**: Writers get priority over readers
- **No starvation**: Both readers and writers are guaranteed to make progress
- **No busy waiting**: Uses condition variables for efficient blocking
- **Circular buffer**: Fixed-size log that overwrites old entries
- **Performance metrics**: Tracks wait times and throughput
- **CSV export**: Optional log dump for debugging

## Files

- `rw_log.h` - API definitions and data structures
- `rwlog.c` - Core monitor implementation with synchronization
- `rw_main.c` - Test harness with thread management and statistics
- `Makefile` - Build configuration

## Building

```bash
make
```

## Usage

```bash
./rw_log [options]

Options:
  -c, --capacity <N>        Log capacity (default 1024)
  -r, --readers <N>         Number of reader threads (default 6)
  -w, --writers <N>         Number of writer threads (default 4)
  -b, --writer-batch <N>    Entries written per writer section (default 2)
  -s, --seconds <N>         Total run time (default 10)
  -R, --rd-us <usec>        Reader sleep between operations (default 2000)
  -W, --wr-us <usec>        Writer sleep between operations (default 3000)
  -d, --dump                Dump final log to log.csv
  -h, --help                Show help message
```

## Example

```bash
./rw_log --capacity 1024 --readers 6 --writers 4 --writer-batch 2 --seconds 10 --rd-us 2000 --wr-us 3000 --dump
```

## Implementation Details

### Synchronization Policy

The implementation uses a **writer-preference** policy:

1. **Multiple readers** can read simultaneously when no writer is active
2. **New readers are blocked** if:
   - A writer is currently active, OR
   - Any writer is waiting to write
3. **Only one writer** can write at a time
4. **No starvation**: Writers and readers are guaranteed to make progress

#### Synchronization State Variables
- `readers_active`: Number of threads currently reading
- `readers_waiting`: Number of readers blocked waiting to read
- `writers_waiting`: Number of writers blocked waiting to write
- `writer_active`: Boolean flag indicating if a writer is currently active

### Data Structures

#### Monitor Structure (`rwlog_monitor_t`)
```c
typedef struct {
    pthread_mutex_t mutex;           // Protects all shared state
    pthread_cond_t read_cond;       // Readers wait on this
    pthread_cond_t write_cond;      // Writers wait on this
    
    // Synchronization state
    int readers_active;
    int readers_waiting;
    int writers_waiting;
    int writer_active;
    
    // Circular buffer state
    rwlog_entry_t *entries;         // Shared memory buffer
    size_t capacity;                 // Buffer size
    size_t head;                     // Next write position
    size_t tail;                     // Oldest entry position
    size_t count;                    // Current entry count
    uint64_t next_seq;               // Next sequence number
} rwlog_monitor_t;
```

#### Log Entry Structure (`rwlog_entry_t`)
```c
typedef struct {
    uint64_t      seq;                // Global, monotonically increasing
    pthread_t     tid;                // Writing thread's pthread id
    struct timespec ts;               // Timestamp (CLOCK_REALTIME)
    char          msg[RWLOG_MSG_MAX]; // Writer-supplied message
} rwlog_entry_t;
```

### Thread Safety Mechanisms

#### Critical Section Protection
- **Single mutex** (`monitor->mutex`) protects all shared state
- **Condition variables** eliminate busy waiting:
  - `read_cond`: Readers wait when writers are active/waiting
  - `write_cond`: Writers wait when readers are active or another writer is active

#### Writer-Preference Algorithm
```c
// Reader entry logic
while (monitor->writer_active || monitor->writers_waiting > 0) {
    monitor->readers_waiting++;
    pthread_cond_wait(&monitor->read_cond, &monitor->mutex);
    monitor->readers_waiting--;
}

// Writer entry logic  
while (monitor->readers_active > 0 || monitor->writer_active) {
    pthread_cond_wait(&monitor->write_cond, &monitor->mutex);
}
```

#### Signal/Broadcast Strategy
- **Writers signal other writers** when exiting (writer preference)
- **Writers broadcast to all readers** when no writers are waiting
- **Readers signal writers** when the last reader exits

### Shared Memory Implementation

#### POSIX Shared Memory
- **Creation**: `shm_open()` with `/rwlog_shm` name
- **Sizing**: `ftruncate()` to set buffer size
- **Mapping**: `mmap()` for direct memory access
- **Cleanup**: `munmap()`, `close()`, `shm_unlink()`

#### Circular Buffer Logic
- **Head pointer**: Next position to write
- **Tail pointer**: Oldest entry position
- **Wraparound**: `buffer_index(pos, capacity) = pos % capacity`
- **Overflow handling**: When full, tail advances to maintain sequence order

### Performance Optimizations

#### Efficient Synchronization
- **No busy waiting**: All waits use `pthread_cond_wait()`
- **Appropriate signaling**: `pthread_cond_signal()` vs `pthread_cond_broadcast()`
- **Batch operations**: Writers can append multiple entries per critical section

#### Memory Management
- **Shared memory**: Eliminates data copying between processes
- **Circular buffer**: Fixed memory footprint regardless of log size
- **Sequence numbers**: Enable efficient ordering without sorting

### Performance Metrics

The program tracks and reports:
- **Average writer wait time** (milliseconds)
- **Average reader critical section time** (milliseconds)  
- **Total entries written**
- **Throughput** (entries per second)

#### Metrics Collection
- **Thread-local timing**: Each thread measures its own wait times
- **Global counters**: Shared statistics protected by separate mutex
- **Real-time calculation**: Metrics computed during execution

## Testing

Run a quick test:
```bash
make test
```

This runs a 5-second test with 2 readers and 2 writers, generating a CSV log file.

## Error Handling

The implementation includes comprehensive error handling:
- All system calls check return values
- Graceful cleanup on errors
- Clear error messages to stderr
- Proper resource deallocation

## Cleanup

```bash
make clean
```

This removes compiled objects, the executable, and any generated CSV files.