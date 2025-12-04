/*
 * Virtual Memory Manager
 * CPSC 380 - Assignment 6
 * 
 * This program translates logical addresses to physical addresses
 * using a TLB and page table, implementing demand paging with
 * page replacement.
 */

#include <stdio.h>
#include <stdlib.h>

/* Constants */
#define PAGE_SIZE 256              // 2^8 bytes per page
#define TLB_SIZE 16                // Number of entries in TLB
#define NUM_FRAMES 128             // Number of frames (for page replacement phase)
#define PHYSICAL_MEM_SIZE (NUM_FRAMES * PAGE_SIZE)  // Total physical memory
#define VIRTUAL_ADDR_SPACE 65536   // 2^16 bytes
#define NUM_PAGES 256              // 2^8 pages (8-bit page number)

/* TLB Entry Structure */
typedef struct {
    int page_num;
    int frame_num;
    int valid;      // 1 if valid, 0 if invalid
    int access_time; // For LRU replacement
} tlb_entry_t;

/* Page Table Entry Structure */
typedef struct {
    int frame_num;
    int valid;      // 1 if valid, 0 if invalid
} page_table_entry_t;

/* Physical Memory */
char physical_memory[PHYSICAL_MEM_SIZE];

/* Page Table */
page_table_entry_t page_table[NUM_PAGES];

/* TLB */
tlb_entry_t tlb[TLB_SIZE];

/* Statistics */
int total_addresses = 0;
int page_faults = 0;
int tlb_hits = 0;
int tlb_misses = 0;

/* Global time counter for LRU */
int current_time = 0;

/* FIFO queue for page replacement */
int frame_queue[NUM_FRAMES];
int queue_head = 0;
int queue_tail = 0;
int frames_used = 0;

/* Function Prototypes */
void init_page_table(void);
void init_tlb(void);
int check_tlb(int page_num, int *frame_num);
void update_tlb(int page_num, int frame_num);
int check_page_table(int page_num, int *frame_num);
void update_page_table(int page_num, int frame_num);
int handle_page_fault(int page_num, FILE *backing_store);
int get_free_frame(void);
void load_page_from_backing_store(int page_num, int frame_num, FILE *backing_store);
int translate_address(int logical_addr, FILE *backing_store, int *physical_addr_out);
void print_statistics(void);

/*
 * Initialize page table - all entries invalid
 */
void init_page_table(void) {
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].frame_num = -1;
        page_table[i].valid = 0;
    }
}

/*
 * Initialize TLB - all entries invalid
 */
void init_tlb(void) {
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].page_num = -1;
        tlb[i].frame_num = -1;
        tlb[i].valid = 0;
        tlb[i].access_time = 0;
    }
}

/*
 * Check TLB for page number
 * Returns 1 if TLB hit, 0 if miss
 * If hit, frame_num is set to the frame number
 */
int check_tlb(int page_num, int *frame_num) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page_num == page_num) {
            *frame_num = tlb[i].frame_num;
            tlb[i].access_time = current_time++;
            tlb_hits++;
            return 1;  // TLB hit
        }
    }
    tlb_misses++;
    return 0;  // TLB miss
}

/*
 * Update TLB with new entry using LRU replacement
 */
void update_tlb(int page_num, int frame_num) {
    // First, try to find an invalid entry
    for (int i = 0; i < TLB_SIZE; i++) {
        if (!tlb[i].valid) {
            tlb[i].page_num = page_num;
            tlb[i].frame_num = frame_num;
            tlb[i].valid = 1;
            tlb[i].access_time = current_time++;
            return;
        }
    }
    
    // TLB is full, find LRU entry (smallest access_time)
    int lru_index = 0;
    int min_time = tlb[0].access_time;
    for (int i = 1; i < TLB_SIZE; i++) {
        if (tlb[i].access_time < min_time) {
            min_time = tlb[i].access_time;
            lru_index = i;
        }
    }
    
    // Replace LRU entry
    tlb[lru_index].page_num = page_num;
    tlb[lru_index].frame_num = frame_num;
    tlb[lru_index].valid = 1;
    tlb[lru_index].access_time = current_time++;
}

/*
 * Check page table for page number
 * Returns 1 if found, 0 if page fault
 */
int check_page_table(int page_num, int *frame_num) {
    if (page_num < 0 || page_num >= NUM_PAGES) {
        return 0;
    }
    
    if (page_table[page_num].valid) {
        *frame_num = page_table[page_num].frame_num;
        return 1;
    }
    
    return 0;  // Page fault
}

/*
 * Update page table with new mapping
 */
void update_page_table(int page_num, int frame_num) {
    if (page_num >= 0 && page_num < NUM_PAGES) {
        page_table[page_num].frame_num = frame_num;
        page_table[page_num].valid = 1;
    }
}

/*
 * Get a free frame using FIFO replacement if necessary
 */
int get_free_frame(void) {
    // If we haven't used all frames yet, allocate a new one
    if (frames_used < NUM_FRAMES) {
        int frame = frames_used;
        frame_queue[queue_tail] = frame;
        queue_tail = (queue_tail + 1) % NUM_FRAMES;
        frames_used++;
        return frame;
    }
    
    // All frames are used, use FIFO to replace
    int frame_to_replace = frame_queue[queue_head];
    queue_head = (queue_head + 1) % NUM_FRAMES;
    
    // Invalidate the page table entry for the replaced frame
    for (int i = 0; i < NUM_PAGES; i++) {
        if (page_table[i].valid && page_table[i].frame_num == frame_to_replace) {
            page_table[i].valid = 0;
            // Also remove from TLB if present
            for (int j = 0; j < TLB_SIZE; j++) {
                if (tlb[j].valid && tlb[j].page_num == i) {
                    tlb[j].valid = 0;
                }
            }
            break;
        }
    }
    
    // Add to queue
    frame_queue[queue_tail] = frame_to_replace;
    queue_tail = (queue_tail + 1) % NUM_FRAMES;
    
    return frame_to_replace;
}

/*
 * Load a page from backing store into physical memory
 */
void load_page_from_backing_store(int page_num, int frame_num, FILE *backing_store) {
    // Calculate offset in backing store (page_num * PAGE_SIZE)
    long offset = (long)page_num * PAGE_SIZE;
    
    // Seek to the correct position
    if (fseek(backing_store, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Could not seek to position %ld in backing store\n", offset);
        exit(1);
    }
    
    // Read the page into physical memory
    char *frame_start = physical_memory + (frame_num * PAGE_SIZE);
    if (fread(frame_start, sizeof(char), PAGE_SIZE, backing_store) != PAGE_SIZE) {
        fprintf(stderr, "Error: Could not read page %d from backing store\n", page_num);
        exit(1);
    }
}

/*
 * Handle page fault by loading page from backing store
 */
int handle_page_fault(int page_num, FILE *backing_store) {
    page_faults++;
    
    // Get a free frame (or replace one using FIFO)
    int frame_num = get_free_frame();
    
    // Load page from backing store
    load_page_from_backing_store(page_num, frame_num, backing_store);
    
    // Update page table
    update_page_table(page_num, frame_num);
    
    // Update TLB
    update_tlb(page_num, frame_num);
    
    return frame_num;
}

/*
 * Translate logical address to physical address
 * Returns the signed byte value at the physical address
 * Physical address is stored in *physical_addr_out
 */
int translate_address(int logical_addr, FILE *backing_store, int *physical_addr_out) {
    // Mask to get only 16 bits
    logical_addr = logical_addr & 0xFFFF;
    
    // Extract page number (bits 8-15) and offset (bits 0-7)
    int page_num = (logical_addr >> 8) & 0xFF;
    int offset = logical_addr & 0xFF;
    
    int frame_num;
    int tlb_hit = check_tlb(page_num, &frame_num);
    
    if (!tlb_hit) {
        // TLB miss - check page table
        int page_table_hit = check_page_table(page_num, &frame_num);
        
        if (!page_table_hit) {
            // Page fault - load from backing store
            frame_num = handle_page_fault(page_num, backing_store);
        } else {
            // Page table hit - update TLB
            update_tlb(page_num, frame_num);
        }
    }
    
    // Calculate physical address
    int physical_addr = (frame_num * PAGE_SIZE) + offset;
    *physical_addr_out = physical_addr;
    
    // Read and return the signed byte value
    char *byte_ptr = physical_memory + physical_addr;
    return (int)(signed char)(*byte_ptr);
}

/*
 * Print statistics after processing all addresses
 */
void print_statistics(void) {
    printf("\n");
    printf("Page-fault rate: %.2f%%\n", 
           (double)page_faults / total_addresses * 100.0);
    printf("TLB hit rate: %.2f%%\n", 
           (double)tlb_hits / total_addresses * 100.0);
}

/*
 * Main function
 */
int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <address_file>\n", argv[0]);
        fprintf(stderr, "Example: %s addresses.txt\n", argv[0]);
        exit(1);
    }
    
    // Open address file
    FILE *addr_file = fopen(argv[1], "r");
    if (addr_file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        exit(1);
    }
    
    // Open backing store
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "Error: Could not open BACKING_STORE.bin\n");
        fclose(addr_file);
        exit(1);
    }
    
    // Initialize data structures
    init_page_table();
    init_tlb();
    
    // Read and process addresses
    int logical_addr;
    while (fscanf(addr_file, "%d", &logical_addr) == 1) {
        total_addresses++;
        
        // Translate address
        int physical_addr;
        int byte_value = translate_address(logical_addr, backing_store, &physical_addr);
        
        // Output results
        printf("Virtual address: %5d Physical address: %5d Value: %4d\n",
               logical_addr, physical_addr, byte_value);
    }
    
    // Print statistics
    print_statistics();
    
    // Clean up
    fclose(addr_file);
    fclose(backing_store);
    
    return 0;
}

