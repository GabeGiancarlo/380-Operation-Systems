# Virtual Memory Manager

A virtual memory manager implementation that translates logical addresses to physical addresses using a Translation Lookaside Buffer (TLB) and page table, implementing demand paging with page replacement algorithms.

## Features

- **Address Translation**: Converts 16-bit logical addresses to physical addresses
- **TLB (Translation Lookaside Buffer)**: 16-entry TLB with LRU replacement policy
- **Page Table**: Manages page-to-frame mappings for 256 pages
- **Demand Paging**: Loads pages from backing store on page faults
- **Page Replacement**: FIFO replacement policy when physical memory is full (128 frames)
- **Statistics**: Reports page-fault rate and TLB hit rate

## Requirements

- C compiler with C99 support (gcc recommended)
- Standard C library
- `BACKING_STORE.bin` file (65,536 bytes)
- `addresses.txt` file with logical addresses

## Compilation

```bash
make
```

Or compile manually:

```bash
gcc -Wall -Wextra -std=c99 -O2 -g -o vmmgr vmmgr.c
```

## Usage

### Running the Program

The program requires an address file as a command-line argument:

```bash
./vmmgr <address_file>
```

**Example:**
```bash
./vmmgr addresses.txt
```

### Program Output

For each logical address, the program outputs:
- Virtual address (logical address)
- Physical address (translated address)
- Value (signed byte value at the physical address)

After processing all addresses, the program reports:
- Page-fault rate (percentage)
- TLB hit rate (percentage)

### Example Output

```
Virtual address: 16916 Physical address:   20 Value:    0
Virtual address: 62493 Physical address:  253 Value:   -1
Virtual address: 30198 Physical address:  118 Value:  118
Virtual address: 53683 Physical address:   83 Value:   83
Virtual address: 40185 Physical address:  185 Value: -123
...
Virtual address: 12107 Physical address:  107 Value:  107

Page-fault rate: 12.50%
TLB hit rate: 3.20%
```

## Architecture

### Address Structure

- **16-bit logical addresses** (masked from 32-bit input)
- **8-bit page number** (bits 8-15)
- **8-bit page offset** (bits 0-7)

### Memory Configuration

- **Page size**: 256 bytes (2^8)
- **Virtual address space**: 65,536 bytes (2^16)
- **Number of pages**: 256 (2^8)
- **Physical memory**: 32,768 bytes (128 frames × 256 bytes)
- **Number of frames**: 128 (for page replacement phase)
- **TLB size**: 16 entries
- **Page table size**: 256 entries (one per page)

### Address Translation Process

1. **Extract page number and offset** from logical address
2. **Check TLB** for page number
   - If TLB hit: Get frame number from TLB
   - If TLB miss: Continue to step 3
3. **Check page table** for page number
   - If page table hit: Get frame number, update TLB
   - If page fault: Continue to step 4
4. **Handle page fault**:
   - Get free frame (or replace using FIFO)
   - Read 256-byte page from `BACKING_STORE.bin`
   - Update page table and TLB
5. **Calculate physical address**: `frame_number × PAGE_SIZE + offset`
6. **Read byte value** from physical memory

### TLB Management

- **Size**: 16 entries
- **Replacement Policy**: LRU (Least Recently Used)
- **Update**: TLB is updated on both TLB hits and page table hits

### Page Replacement

- **Policy**: FIFO (First In, First Out)
- **Trigger**: When all 128 frames are in use
- **Process**:
  1. Select oldest frame (head of FIFO queue)
  2. Invalidate page table entry for replaced page
  3. Remove from TLB if present
  4. Load new page into frame
  5. Update page table and TLB

## File Structure

- `vmmgr.c`: Main program source code
- `addresses.txt`: Input file with logical addresses (one per line)
- `BACKING_STORE.bin`: Binary backing store file (65,536 bytes)
- `Makefile`: Build configuration
- `README.md`: This file

## Testing

### Basic Test

```bash
make test
```

This will compile and run the program with `addresses.txt`.

### Manual Testing

```bash
./vmmgr addresses.txt
```

The program processes 1,000 addresses from the input file and outputs the translation results along with statistics.

## Implementation Details

### Data Structures

- **TLB Entry**: Stores page number, frame number, validity flag, and access time (for LRU)
- **Page Table Entry**: Stores frame number and validity flag
- **Physical Memory**: Array of 32,768 bytes (128 frames × 256 bytes)
- **FIFO Queue**: Circular queue for tracking frame replacement order

### Key Functions

- `init_page_table()`: Initialize all page table entries as invalid
- `init_tlb()`: Initialize all TLB entries as invalid
- `check_tlb()`: Search TLB for page number, return frame if found
- `update_tlb()`: Add entry to TLB using LRU replacement
- `check_page_table()`: Check if page is in page table
- `update_page_table()`: Add or update page table entry
- `handle_page_fault()`: Load page from backing store into physical memory
- `get_free_frame()`: Allocate new frame or replace using FIFO
- `load_page_from_backing_store()`: Read 256 bytes from backing store file
- `translate_address()`: Main translation function
- `print_statistics()`: Output page-fault rate and TLB hit rate

### Error Handling

The program handles:
- Missing command-line arguments
- File open errors (address file and backing store)
- File seek/read errors
- Invalid page numbers

## Statistics

### Page-Fault Rate

Percentage of address references that resulted in page faults:

```
Page-fault rate = (page_faults / total_addresses) × 100%
```

### TLB Hit Rate

Percentage of address references resolved in the TLB:

```
TLB hit rate = (tlb_hits / total_addresses) × 100%
```

## Assumptions

1. Logical addresses are 32-bit integers, but only the rightmost 16 bits are used
2. The backing store file (`BACKING_STORE.bin`) is exactly 65,536 bytes
3. Physical memory has 128 frames (32,768 bytes total)
4. Page size is 256 bytes
5. All addresses in the input file are valid (0-65535)
6. The backing store file exists and is readable

## Limitations

- Maximum 256 pages in virtual address space
- Physical memory limited to 128 frames
- TLB limited to 16 entries
- No write support (read-only memory access)

## Code Structure

The code is organized into the following sections:

1. **Constants and Data Structures**: TLB entry, page table entry, physical memory
2. **Initialization**: `init_page_table()`, `init_tlb()`
3. **TLB Operations**: `check_tlb()`, `update_tlb()`
4. **Page Table Operations**: `check_page_table()`, `update_page_table()`
5. **Page Fault Handling**: `handle_page_fault()`, `get_free_frame()`, `load_page_from_backing_store()`
6. **Address Translation**: `translate_address()`
7. **Statistics**: `print_statistics()`
8. **Main Function**: File I/O and address processing loop

## Author

Gabriel Giancarlo

## License

This is an academic project for CPSC 380 - Operating Systems.

