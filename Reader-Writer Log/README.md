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

### Data Structures

- **Circular buffer**: Fixed-size array with head/tail pointers
- **Shared memory**: POSIX shared memory for inter-process access
- **Synchronization primitives**: Mutex and condition variables

### Thread Safety

- All critical sections are protected by mutexes
- Condition variables used for efficient blocking (no busy waiting)
- Writer-preference logic ensures proper ordering
- Sequence numbers provide monotonicity guarantees

### Performance Metrics

The program tracks and reports:
- **Average writer wait time** (milliseconds)
- **Average reader critical section time** (milliseconds)
- **Total entries written**
- **Throughput** (entries per second)

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
