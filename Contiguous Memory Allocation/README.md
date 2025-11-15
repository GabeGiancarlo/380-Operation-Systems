# Contiguous Memory Allocator

A comprehensive contiguous memory allocation simulator that implements three allocation strategies (First Fit, Best Fit, Worst Fit) with memory compaction, fragmentation analysis, and visual memory mapping.

## Features

- **Three Allocation Strategies:**
  - **First Fit (F)**: Allocates from the first hole large enough to satisfy the request
  - **Best Fit (B)**: Allocates from the smallest hole large enough to satisfy the request
  - **Worst Fit (W)**: Allocates from the largest available hole

- **Memory Management:**
  - Automatic hole merging when adjacent free blocks are created
  - Memory compaction to consolidate all free memory into one block
  - Process-based memory allocation and release

- **Statistics and Analysis:**
  - Detailed memory status reports (allocated and free regions)
  - Fragmentation metrics (external fragmentation percentage)
  - Average hole size calculation
  - Visual memory map showing allocation patterns

- **Batch Execution:**
  - Execute commands from a file using the SIM command
  - Useful for testing and automation

## Requirements

- C compiler with C99 support (gcc recommended)
- Standard C library (stdlib.h, stdio.h, string.h, etc.)

## Compilation

```bash
make
```

Or compile manually:

```bash
gcc -Wall -Wextra -std=c99 -O2 -g -o allocator allocator.c
```

## Usage

### Starting the Allocator

The allocator requires a memory size as a command-line argument:

```bash
./allocator <memory_size>
```

**Example:**
```bash
./allocator 1048576
```

This initializes the allocator with 1,048,576 bytes (1 MB) of memory.

### Interactive Commands

Once started, the program displays a prompt and accepts the following commands:

| Command | Description | Example |
|---------|-------------|---------|
| `RQ <process> <size> <F\|B\|W>` | Request memory allocation | `RQ P0 40000 W` |
| `RL <process>` | Release memory allocated to a process | `RL P0` |
| `C` | Compact all unused holes into one region | `C` |
| `STAT` | Display memory status report | `STAT` |
| `STAT -v` | Display status with visual memory map | `STAT -v` |
| `SIM <filename>` | Execute commands from a file | `SIM trace.txt` |
| `X` | Exit the program | `X` |

### Command Details

#### RQ (Request Memory)

Allocates a contiguous block of memory for a process using the specified strategy.

**Format:** `RQ <process> <size> <F|B|W>`

- `<process>`: Process name (alphanumeric and underscore characters)
- `<size>`: Size in bytes (supports KB, MB suffixes)
- `<F|B|W>`: Allocation strategy (F=First Fit, B=Best Fit, W=Worst Fit)

**Examples:**
```bash
allocator>RQ P0 40000 F
allocator>RQ P1 50000 B
allocator>RQ P2 60000 W
allocator>RQ P3 100 KB W
```

**Error Handling:**
- Invalid command format
- Duplicate process names
- Insufficient memory
- Invalid size or process name

#### RL (Release Memory)

Releases memory allocated to a process. Adjacent holes are automatically merged.

**Format:** `RL <process>`

**Example:**
```bash
allocator>RL P0
```

**Error Handling:**
- Process not found

#### C (Compact)

Compacts all allocated memory blocks to the beginning of memory, merging all free space into one large hole at the end.

**Format:** `C`

**Example:**
```bash
allocator>C
```

#### STAT (Status Report)

Displays a detailed report of memory allocation status.

**Format:** `STAT` or `STAT -v`

**Example Output:**
```
Allocated memory:
Process P0: Start = 0 KB, End = 40 KB, Size = 40 KB
Process P2: Start = 120 KB, End = 200 KB, Size = 80 KB
Free memory:
Hole 1: Start = 40 KB, End = 120 KB, Size = 80 KB
Hole 2: Start = 200 KB, End = 1024 KB, Size = 824 KB
Summary:
Total allocated: 120 KB
Total free: 904 KB
Largest hole: 824 KB
External fragmentation: 8.8% (1-largest free block/total free memory)
Average hole size: 452 KB
```

**With Visualization (`STAT -v`):**
```
[#####.....##........####]
^0                      ^1048576
```

- `#` represents allocated memory regions
- `.` represents free memory (holes)

Each character in the 50-character bar corresponds to approximately 2% of total memory.

#### SIM (Simulation Mode)

Executes a sequence of commands from a file.

**Format:** `SIM <filename>`

**Example trace file (`trace.txt`):**
```
RQ P1 40000 F
RQ P2 50000 B
RL P1
RQ P3 60000 W
STAT
C
STAT -v
```

**Usage:**
```bash
allocator>SIM trace.txt
```

The program executes all commands in order and prints results as in interactive mode.

#### X (Exit)

Exits the program.

**Format:** `X`

## Allocation Strategies

### First Fit (F)

Allocates from the first hole large enough to satisfy the request.

**Characteristics:**
- Fast allocation (stops at first suitable hole)
- May leave small fragments at the beginning
- Simple implementation

**Best for:** General-purpose allocation when speed is important

### Best Fit (B)

Allocates from the smallest hole large enough to satisfy the request.

**Characteristics:**
- Minimizes wasted space in each allocation
- May create many small fragments
- Requires scanning all holes

**Best for:** Minimizing internal fragmentation

### Worst Fit (W)

Allocates from the largest available hole.

**Characteristics:**
- Leaves large remaining holes
- May reduce fragmentation in some cases
- Requires scanning all holes

**Best for:** Scenarios where large free blocks are beneficial

## Fragmentation Metrics

### External Fragmentation

External fragmentation is calculated as:

```
External Fragmentation = (1 - largest_free_block / total_free_memory) × 100%
```

This metric indicates how fragmented the free memory is. Lower values indicate less fragmentation.

### Average Hole Size

The average size of all free memory holes:

```
Average Hole Size = total_free_memory / number_of_holes
```

## Example Session

```bash
$ ./allocator 1048576
allocator>RQ P0 40000 F
allocator>RQ P1 50000 B
allocator>RQ P2 30000 W
allocator>STAT
Allocated memory:
Process P0: Start = 0 KB, End = 40 KB, Size = 40 KB
Process P1: Start = 40 KB, End = 90 KB, Size = 50 KB
Process P2: Start = 90 KB, End = 120 KB, Size = 30 KB
Free memory:
Hole 1: Start = 120 KB, End = 1024 KB, Size = 904 KB
Summary:
Total allocated: 120 KB
Total free: 904 KB
Largest hole: 904 KB
External fragmentation: 0.0% (1-largest free block/total free memory)
Average hole size: 904 KB
allocator>RL P1
allocator>STAT -v
Allocated memory:
Process P0: Start = 0 KB, End = 40 KB, Size = 40 KB
Process P2: Start = 90 KB, End = 120 KB, Size = 30 KB
Free memory:
Hole 1: Start = 40 KB, End = 90 KB, Size = 50 KB
Hole 2: Start = 120 KB, End = 1024 KB, Size = 904 KB
Summary:
Total allocated: 70 KB
Total free: 954 KB
Largest hole: 904 KB
External fragmentation: 5.2% (1-largest free block/total free memory)
Average hole size: 477 KB

[####....####................................]
^0                                      ^1048576
allocator>C
allocator>STAT -v
Allocated memory:
Process P0: Start = 0 KB, End = 40 KB, Size = 40 KB
Process P2: Start = 40 KB, End = 70 KB, Size = 30 KB
Free memory:
Hole 1: Start = 70 KB, End = 1024 KB, Size = 954 KB
Summary:
Total allocated: 70 KB
Total free: 954 KB
Largest hole: 954 KB
External fragmentation: 0.0% (1-largest free block/total free memory)
Average hole size: 954 KB

[#######.................................................]
^0                                      ^1048576
allocator>X
```

## Testing

### Basic Test

Create a test file (`test.txt`):
```
RQ P1 40000 F
RQ P2 50000 B
RL P1
RQ P3 60000 W
STAT
C
STAT -v
X
```

Run the test:
```bash
echo "1048576" | ./allocator < test.txt
```

### Using Make

```bash
make test
```

## Architecture

### Data Structures

- **MemoryBlock**: Represents a contiguous region of memory (allocated or free)
  - Linked list structure maintains blocks in sorted order by address
  - Each block tracks start, end, size, type, and process name

- **Allocator**: Main allocator state
  - Total memory size
  - Linked list of memory blocks
  - Process count

### Memory Layout

Memory blocks are maintained in a sorted linked list by starting address. This allows:
- Efficient insertion and removal
- Easy detection of adjacent blocks for merging
- Simple traversal for allocation strategies

### Hole Merging

When memory is released:
1. The allocated block is converted to a free block
2. The allocator scans for adjacent free blocks
3. Adjacent free blocks are merged into a single larger block

### Compaction

Compaction process:
1. Collect all allocated blocks
2. Free all existing block structures
3. Rebuild memory layout with allocated blocks at the beginning
4. Create one large free block for remaining space

## Error Handling

The program handles various error conditions:
- Invalid command-line arguments
- Invalid command formats
- Duplicate process names
- Releasing non-existent processes
- Insufficient memory for allocation
- Invalid process names or sizes
- File I/O errors (for SIM command)

## Limitations

- Maximum 1000 processes (configurable via MAX_PROCESSES)
- Process names limited to 63 characters
- Memory size must fit in size_t (typically 64-bit on modern systems)

## Code Structure

The code is organized into the following sections:

1. **Data Structures**: MemoryBlock, Allocator
2. **Initialization**: allocator_init(), allocator_cleanup()
3. **Block Management**: create_block(), insert_block(), remove_block(), merge_adjacent_holes()
4. **Allocation Strategies**: allocate_first_fit(), allocate_best_fit(), allocate_worst_fit()
5. **Memory Operations**: release_memory(), compact_memory()
6. **Statistics**: calculate_fragmentation(), print_statistics(), print_visualization()
7. **Command Processing**: process_command(), execute_simulation()
8. **Utilities**: parse_size(), trim_whitespace(), is_valid_process_name()

## Assumptions

1. Memory addresses start at 0 and go up to (total_size - 1)
2. All sizes are specified in bytes (with optional KB/MB suffixes)
3. Process names are case-sensitive
4. The allocator maintains blocks in sorted order by address
5. Compaction preserves process names and sizes but relocates blocks
6. External fragmentation is calculated as specified in the assignment

## Author

Gabriel Giancarlo

## License

This is an academic project for CPSC 380 - Operating Systems.

