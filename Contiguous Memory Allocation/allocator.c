/**
 * @file allocator.c
 * @brief Contiguous Memory Allocator
 * 
 * This program implements a dynamic memory manager that simulates contiguous
 * memory allocation. It supports three allocation strategies:
 * - First Fit (F): Allocate from the first hole large enough
 * - Best Fit (B): Allocate from the smallest hole large enough
 * - Worst Fit (W): Allocate from the largest available hole
 * 
 * Features:
 * - Memory allocation and release with automatic hole merging
 * - Compaction to consolidate all free memory
 * - Fragmentation metrics and statistics
 * - Visual memory map output
 * - Batch command execution from file
 * 
 * @author Gabriel Giancarlo
 * @date 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>

/* ============================================================================
 * CONSTANTS AND MACROS
 * ============================================================================ */

#define MAX_PROCESSES 1000
#define MAX_LINE_LENGTH 256
#define MAX_PROCESS_NAME_LENGTH 64
#define VISUALIZATION_WIDTH 50

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @enum AllocationStrategy
 * @brief Enumeration of supported allocation strategies
 */
typedef enum {
    STRATEGY_FIRST_FIT = 0,
    STRATEGY_BEST_FIT,
    STRATEGY_WORST_FIT
} AllocationStrategy;

/**
 * @enum MemoryBlockType
 * @brief Type of memory block (allocated or free)
 */
typedef enum {
    BLOCK_ALLOCATED,
    BLOCK_FREE
} MemoryBlockType;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @struct MemoryBlock
 * @brief Represents a contiguous region of memory (allocated or free)
 */
typedef struct MemoryBlock {
    size_t start;                    ///< Starting address
    size_t end;                      ///< Ending address (exclusive)
    size_t size;                     ///< Size of the block
    MemoryBlockType type;            ///< Type: allocated or free
    char process_name[MAX_PROCESS_NAME_LENGTH]; ///< Process name (if allocated)
    struct MemoryBlock *next;        ///< Next block in the list
} MemoryBlock;

/**
 * @struct Allocator
 * @brief Main allocator state structure
 */
typedef struct Allocator {
    size_t total_size;               ///< Total memory size
    MemoryBlock *blocks;              ///< Linked list of memory blocks
    int process_count;               ///< Number of allocated processes
} Allocator;

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

static Allocator g_allocator;        ///< Global allocator instance

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

// Initialization and cleanup
static void allocator_init(Allocator *alloc, size_t total_size);
static void allocator_cleanup(Allocator *alloc);

// Memory block management
static MemoryBlock *create_block(size_t start, size_t end, MemoryBlockType type, const char *process_name);
static void free_block(MemoryBlock *block);
static void insert_block(Allocator *alloc, MemoryBlock *new_block);
static void merge_adjacent_holes(Allocator *alloc);

// Allocation strategies
static MemoryBlock *allocate_first_fit(Allocator *alloc, size_t size, const char *process_name);
static MemoryBlock *allocate_best_fit(Allocator *alloc, size_t size, const char *process_name);
static MemoryBlock *allocate_worst_fit(Allocator *alloc, size_t size, const char *process_name);
static MemoryBlock *allocate_memory(Allocator *alloc, size_t size, const char *process_name, AllocationStrategy strategy);

// Memory operations
static int release_memory(Allocator *alloc, const char *process_name);
static void compact_memory(Allocator *alloc);

// Statistics and reporting
static void print_statistics(Allocator *alloc, bool visualize);
static void print_visualization(Allocator *alloc);
static void calculate_fragmentation(Allocator *alloc, size_t *total_allocated, size_t *total_free, 
                                    size_t *largest_hole, double *external_frag, double *avg_hole_size);

// Command parsing and execution
static void process_command(Allocator *alloc, const char *command);
static void execute_simulation(Allocator *alloc, const char *filename);
static AllocationStrategy parse_strategy(char strategy_char);

// Utility functions
static size_t parse_size(const char *str);
static void trim_whitespace(char *str);
static bool is_valid_process_name(const char *name);

/* ============================================================================
 * MAIN FUNCTION
 * ============================================================================ */

/**
 * @brief Main entry point of the program
 * 
 * Initializes the allocator with the specified memory size and enters
 * the interactive command loop.
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc, char *argv[]) {
    // Check for correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory_size>\n", argv[0]);
        fprintf(stderr, "Example: %s 1048576\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Parse memory size
    size_t memory_size = parse_size(argv[1]);
    if (memory_size == 0) {
        fprintf(stderr, "Error: Invalid memory size '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }
    
    // Initialize allocator
    allocator_init(&g_allocator, memory_size);
    
    // Interactive command loop
    char line[MAX_LINE_LENGTH];
    printf("allocator>");
    fflush(stdout);
    
    while (fgets(line, sizeof(line), stdin) != NULL) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            printf("allocator>");
            fflush(stdout);
            continue;
        }
        
        // Process command
        process_command(&g_allocator, line);
        
        // Check if we should exit
        if (strcmp(line, "X") == 0) {
            break;
        }
        
        printf("allocator>");
        fflush(stdout);
    }
    
    // Cleanup
    allocator_cleanup(&g_allocator);
    
    return EXIT_SUCCESS;
}

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================ */

/**
 * @brief Initialize the allocator with a single free block
 * 
 * @param alloc Pointer to Allocator structure
 * @param total_size Total memory size to manage
 */
static void allocator_init(Allocator *alloc, size_t total_size) {
    memset(alloc, 0, sizeof(Allocator));
    alloc->total_size = total_size;
    alloc->blocks = NULL;
    alloc->process_count = 0;
    
    // Create initial free block covering entire memory
    MemoryBlock *initial_block = create_block(0, total_size, BLOCK_FREE, "");
    if (initial_block == NULL) {
        fprintf(stderr, "Error: Failed to initialize allocator\n");
        exit(EXIT_FAILURE);
    }
    alloc->blocks = initial_block;
}

/**
 * @brief Cleanup allocator and free all memory blocks
 * 
 * @param alloc Pointer to Allocator structure
 */
static void allocator_cleanup(Allocator *alloc) {
    MemoryBlock *current = alloc->blocks;
    while (current != NULL) {
        MemoryBlock *next = current->next;
        free_block(current);
        current = next;
    }
    alloc->blocks = NULL;
}

/* ============================================================================
 * MEMORY BLOCK MANAGEMENT
 * ============================================================================ */

/**
 * @brief Create a new memory block
 * 
 * @param start Starting address
 * @param end Ending address (exclusive)
 * @param type Block type (allocated or free)
 * @param process_name Process name (empty string for free blocks)
 * @return Pointer to new MemoryBlock, or NULL on error
 */
static MemoryBlock *create_block(size_t start, size_t end, MemoryBlockType type, const char *process_name) {
    MemoryBlock *block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
    if (block == NULL) {
        return NULL;
    }
    
    block->start = start;
    block->end = end;
    block->size = end - start;
    block->type = type;
    block->next = NULL;
    
    if (process_name != NULL) {
        strncpy(block->process_name, process_name, MAX_PROCESS_NAME_LENGTH - 1);
        block->process_name[MAX_PROCESS_NAME_LENGTH - 1] = '\0';
    } else {
        block->process_name[0] = '\0';
    }
    
    return block;
}

/**
 * @brief Free a memory block
 * 
 * @param block Pointer to MemoryBlock to free
 */
static void free_block(MemoryBlock *block) {
    if (block != NULL) {
        free(block);
    }
}

/**
 * @brief Insert a block into the sorted linked list
 * 
 * Blocks are maintained in sorted order by starting address.
 * 
 * @param alloc Pointer to Allocator structure
 * @param new_block Block to insert
 */
static void insert_block(Allocator *alloc, MemoryBlock *new_block) {
    if (alloc->blocks == NULL) {
        alloc->blocks = new_block;
        new_block->next = NULL;
        return;
    }
    
    // Insert at head if before first block
    if (new_block->start < alloc->blocks->start) {
        new_block->next = alloc->blocks;
        alloc->blocks = new_block;
        return;
    }
    
    // Find insertion point
    MemoryBlock *current = alloc->blocks;
    while (current->next != NULL && current->next->start < new_block->start) {
        current = current->next;
    }
    
    new_block->next = current->next;
    current->next = new_block;
}


/**
 * @brief Merge adjacent free blocks (holes)
 * 
 * After a release operation, adjacent free blocks should be merged
 * into a single larger free block.
 * 
 * @param alloc Pointer to Allocator structure
 */
static void merge_adjacent_holes(Allocator *alloc) {
    MemoryBlock *current = alloc->blocks;
    
    while (current != NULL && current->next != NULL) {
        // If current and next are both free and adjacent
        if (current->type == BLOCK_FREE && 
            current->next->type == BLOCK_FREE &&
            current->end == current->next->start) {
            
            // Merge next into current
            current->end = current->next->end;
            current->size = current->end - current->start;
            
            // Remove next block
            MemoryBlock *to_remove = current->next;
            current->next = to_remove->next;
            free_block(to_remove);
            
            // Don't advance - check if we can merge again
        } else {
            current = current->next;
        }
    }
}

/* ============================================================================
 * ALLOCATION STRATEGIES
 * ============================================================================ */

/**
 * @brief First Fit allocation strategy
 * 
 * Allocates from the first hole large enough to satisfy the request.
 * 
 * @param alloc Pointer to Allocator structure
 * @param size Size to allocate
 * @param process_name Name of the process requesting memory
 * @return Pointer to allocated block, or NULL if allocation fails
 */
static MemoryBlock *allocate_first_fit(Allocator *alloc, size_t size, const char *process_name) {
    MemoryBlock *current = alloc->blocks;
    
    while (current != NULL) {
        if (current->type == BLOCK_FREE && current->size >= size) {
            // Found a suitable hole
            size_t allocated_start = current->start;
            size_t allocated_end = allocated_start + size;
            
            // Update current block (shrink it)
            if (current->size == size) {
                // Exact fit - convert entire block to allocated
                current->type = BLOCK_ALLOCATED;
                strncpy(current->process_name, process_name, MAX_PROCESS_NAME_LENGTH - 1);
                current->process_name[MAX_PROCESS_NAME_LENGTH - 1] = '\0';
                return current;
            } else {
                // Partial fit - split the block
                current->start = allocated_end;
                current->size = current->end - current->start;
                
                // Create new allocated block
                MemoryBlock *allocated = create_block(allocated_start, allocated_end, 
                                                      BLOCK_ALLOCATED, process_name);
                if (allocated == NULL) {
                    // Restore original block on failure
                    current->start = allocated_start;
                    current->size = current->end - current->start;
                    return NULL;
                }
                
                // Insert allocated block before current
                if (alloc->blocks == current) {
                    allocated->next = current;
                    alloc->blocks = allocated;
                } else {
                    MemoryBlock *prev = alloc->blocks;
                    while (prev->next != current) {
                        prev = prev->next;
                    }
                    prev->next = allocated;
                    allocated->next = current;
                }
                
                return allocated;
            }
        }
        current = current->next;
    }
    
    return NULL; // No suitable hole found
}

/**
 * @brief Best Fit allocation strategy
 * 
 * Allocates from the smallest hole large enough to satisfy the request.
 * 
 * @param alloc Pointer to Allocator structure
 * @param size Size to allocate
 * @param process_name Name of the process requesting memory
 * @return Pointer to allocated block, or NULL if allocation fails
 */
static MemoryBlock *allocate_best_fit(Allocator *alloc, size_t size, const char *process_name) {
    MemoryBlock *best = NULL;
    size_t best_size = SIZE_MAX;
    MemoryBlock *current = alloc->blocks;
    
    // Find the smallest suitable hole
    while (current != NULL) {
        if (current->type == BLOCK_FREE && current->size >= size) {
            if (current->size < best_size) {
                best = current;
                best_size = current->size;
            }
        }
        current = current->next;
    }
    
    if (best == NULL) {
        return NULL; // No suitable hole found
    }
    
    // Allocate from best block
    size_t allocated_start = best->start;
    size_t allocated_end = allocated_start + size;
    
    if (best->size == size) {
        // Exact fit
        best->type = BLOCK_ALLOCATED;
        strncpy(best->process_name, process_name, MAX_PROCESS_NAME_LENGTH - 1);
        best->process_name[MAX_PROCESS_NAME_LENGTH - 1] = '\0';
        return best;
    } else {
        // Partial fit - split the block
        best->start = allocated_end;
        best->size = best->end - best->start;
        
        MemoryBlock *allocated = create_block(allocated_start, allocated_end, 
                                              BLOCK_ALLOCATED, process_name);
        if (allocated == NULL) {
            best->start = allocated_start;
            best->size = best->end - best->start;
            return NULL;
        }
        
        // Insert allocated block
        if (alloc->blocks == best) {
            allocated->next = best;
            alloc->blocks = allocated;
        } else {
            MemoryBlock *prev = alloc->blocks;
            while (prev->next != best) {
                prev = prev->next;
            }
            prev->next = allocated;
            allocated->next = best;
        }
        
        return allocated;
    }
}

/**
 * @brief Worst Fit allocation strategy
 * 
 * Allocates from the largest available hole.
 * 
 * @param alloc Pointer to Allocator structure
 * @param size Size to allocate
 * @param process_name Name of the process requesting memory
 * @return Pointer to allocated block, or NULL if allocation fails
 */
static MemoryBlock *allocate_worst_fit(Allocator *alloc, size_t size, const char *process_name) {
    MemoryBlock *worst = NULL;
    size_t worst_size = 0;
    MemoryBlock *current = alloc->blocks;
    
    // Find the largest hole
    while (current != NULL) {
        if (current->type == BLOCK_FREE && current->size >= size) {
            if (current->size > worst_size) {
                worst = current;
                worst_size = current->size;
            }
        }
        current = current->next;
    }
    
    if (worst == NULL) {
        return NULL; // No suitable hole found
    }
    
    // Allocate from worst block
    size_t allocated_start = worst->start;
    size_t allocated_end = allocated_start + size;
    
    // Always split for worst fit (to leave largest possible remainder)
    worst->start = allocated_end;
    worst->size = worst->end - worst->start;
    
    MemoryBlock *allocated = create_block(allocated_start, allocated_end, 
                                          BLOCK_ALLOCATED, process_name);
    if (allocated == NULL) {
        worst->start = allocated_start;
        worst->size = worst->end - worst->start;
        return NULL;
    }
    
    // Insert allocated block
    if (alloc->blocks == worst) {
        allocated->next = worst;
        alloc->blocks = allocated;
    } else {
        MemoryBlock *prev = alloc->blocks;
        while (prev->next != worst) {
            prev = prev->next;
        }
        prev->next = allocated;
        allocated->next = worst;
    }
    
    return allocated;
}

/**
 * @brief Allocate memory using the specified strategy
 * 
 * @param alloc Pointer to Allocator structure
 * @param size Size to allocate
 * @param process_name Name of the process requesting memory
 * @param strategy Allocation strategy to use
 * @return Pointer to allocated block, or NULL if allocation fails
 */
static MemoryBlock *allocate_memory(Allocator *alloc, size_t size, const char *process_name, AllocationStrategy strategy) {
    switch (strategy) {
        case STRATEGY_FIRST_FIT:
            return allocate_first_fit(alloc, size, process_name);
        case STRATEGY_BEST_FIT:
            return allocate_best_fit(alloc, size, process_name);
        case STRATEGY_WORST_FIT:
            return allocate_worst_fit(alloc, size, process_name);
        default:
            return NULL;
    }
}

/* ============================================================================
 * MEMORY OPERATIONS
 * ============================================================================ */

/**
 * @brief Release memory allocated to a process
 * 
 * Converts the allocated block to a free block and merges adjacent holes.
 * 
 * @param alloc Pointer to Allocator structure
 * @param process_name Name of the process whose memory should be released
 * @return 0 on success, -1 if process not found
 */
static int release_memory(Allocator *alloc, const char *process_name) {
    MemoryBlock *current = alloc->blocks;
    
    while (current != NULL) {
        if (current->type == BLOCK_ALLOCATED && 
            strcmp(current->process_name, process_name) == 0) {
            // Found the process - convert to free block
            current->type = BLOCK_FREE;
            current->process_name[0] = '\0';
            alloc->process_count--;
            
            // Merge adjacent holes
            merge_adjacent_holes(alloc);
            
            return 0;
        }
        current = current->next;
    }
    
    return -1; // Process not found
}

/**
 * @brief Compact memory by moving all allocated blocks to the beginning
 * 
 * All free blocks are merged into one large block at the end.
 * 
 * @param alloc Pointer to Allocator structure
 */
static void compact_memory(Allocator *alloc) {
    // Collect all allocated blocks with their information
    struct {
        char process_name[MAX_PROCESS_NAME_LENGTH];
        size_t size;
    } allocated_info[MAX_PROCESSES];
    int allocated_count = 0;
    
    MemoryBlock *current = alloc->blocks;
    while (current != NULL && allocated_count < MAX_PROCESSES) {
        if (current->type == BLOCK_ALLOCATED) {
            strncpy(allocated_info[allocated_count].process_name, current->process_name, 
                   MAX_PROCESS_NAME_LENGTH - 1);
            allocated_info[allocated_count].process_name[MAX_PROCESS_NAME_LENGTH - 1] = '\0';
            allocated_info[allocated_count].size = current->size;
            allocated_count++;
        }
        current = current->next;
    }
    
    // If no allocated blocks, nothing to do
    if (allocated_count == 0) {
        return;
    }
    
    // Free all existing blocks and rebuild
    current = alloc->blocks;
    while (current != NULL) {
        MemoryBlock *next = current->next;
        free_block(current);
        current = next;
    }
    alloc->blocks = NULL;
    
    // Rebuild: place allocated blocks first, then one free block
    size_t next_start = 0;
    for (int i = 0; i < allocated_count; i++) {
        size_t block_size = allocated_info[i].size;
        
        // Create new allocated block at compacted location
        MemoryBlock *new_block = create_block(next_start, next_start + block_size, 
                                              BLOCK_ALLOCATED, allocated_info[i].process_name);
        if (new_block == NULL) {
            // Error - cleanup and exit
            allocator_cleanup(alloc);
            allocator_init(alloc, alloc->total_size);
            return;
        }
        
        insert_block(alloc, new_block);
        next_start += block_size;
    }
    
    // Create one large free block for remaining space
    if (next_start < alloc->total_size) {
        MemoryBlock *free_block = create_block(next_start, alloc->total_size, BLOCK_FREE, "");
        if (free_block != NULL) {
            insert_block(alloc, free_block);
        }
    }
}

/* ============================================================================
 * STATISTICS AND REPORTING
 * ============================================================================ */

/**
 * @brief Calculate fragmentation metrics
 * 
 * @param alloc Pointer to Allocator structure
 * @param total_allocated Output: total allocated memory
 * @param total_free Output: total free memory
 * @param largest_hole Output: size of largest free block
 * @param external_frag Output: external fragmentation percentage
 * @param avg_hole_size Output: average hole size
 */
static void calculate_fragmentation(Allocator *alloc, size_t *total_allocated, size_t *total_free, 
                                    size_t *largest_hole, double *external_frag, double *avg_hole_size) {
    *total_allocated = 0;
    *total_free = 0;
    *largest_hole = 0;
    int hole_count = 0;
    
    MemoryBlock *current = alloc->blocks;
    while (current != NULL) {
        if (current->type == BLOCK_ALLOCATED) {
            *total_allocated += current->size;
        } else {
            *total_free += current->size;
            if (current->size > *largest_hole) {
                *largest_hole = current->size;
            }
            hole_count++;
        }
        current = current->next;
    }
    
    // Calculate external fragmentation: (1 - largest_free_block / total_free) * 100
    if (*total_free > 0) {
        *external_frag = (1.0 - (double)(*largest_hole) / (double)(*total_free)) * 100.0;
    } else {
        *external_frag = 0.0;
    }
    
    // Calculate average hole size
    if (hole_count > 0) {
        *avg_hole_size = (double)(*total_free) / (double)hole_count;
    } else {
        *avg_hole_size = 0.0;
    }
}

/**
 * @brief Print memory visualization map
 * 
 * @param alloc Pointer to Allocator structure
 */
static void print_visualization(Allocator *alloc) {
    char map[VISUALIZATION_WIDTH + 1];
    map[VISUALIZATION_WIDTH] = '\0';
    
    // Initialize map
    for (int i = 0; i < VISUALIZATION_WIDTH; i++) {
        map[i] = '.';
    }
    
    // Mark allocated regions
    MemoryBlock *current = alloc->blocks;
    while (current != NULL) {
        if (current->type == BLOCK_ALLOCATED) {
            // Calculate which characters this block occupies
            double start_ratio = (double)current->start / (double)alloc->total_size;
            double end_ratio = (double)current->end / (double)alloc->total_size;
            
            int start_char = (int)(start_ratio * VISUALIZATION_WIDTH);
            int end_char = (int)(end_ratio * VISUALIZATION_WIDTH);
            
            // Ensure we don't go out of bounds
            if (start_char < 0) start_char = 0;
            if (end_char > VISUALIZATION_WIDTH) end_char = VISUALIZATION_WIDTH;
            if (start_char >= VISUALIZATION_WIDTH) start_char = VISUALIZATION_WIDTH - 1;
            
            for (int i = start_char; i < end_char; i++) {
                map[i] = '#';
            }
        }
        current = current->next;
    }
    
    printf("[%s]\n", map);
    printf("^0");
    for (int i = 0; i < VISUALIZATION_WIDTH - 2; i++) {
        printf(" ");
    }
    printf("^%zu\n", alloc->total_size);
}

/**
 * @brief Print statistics report
 * 
 * @param alloc Pointer to Allocator structure
 * @param visualize Whether to include visualization
 */
static void print_statistics(Allocator *alloc, bool visualize) {
    // Print allocated memory
    printf("Allocated memory:\n");
    MemoryBlock *current = alloc->blocks;
    int allocated_count = 0;
    while (current != NULL) {
        if (current->type == BLOCK_ALLOCATED) {
            printf("Process %s: Start = %zu KB, End = %zu KB, Size = %zu KB\n",
                   current->process_name,
                   current->start / 1024,
                   current->end / 1024,
                   current->size / 1024);
            allocated_count++;
        }
        current = current->next;
    }
    if (allocated_count == 0) {
        printf("(No allocated memory)\n");
    }
    
    // Print free memory
    printf("Free memory:\n");
    current = alloc->blocks;
    int hole_num = 1;
    int hole_count = 0;
    while (current != NULL) {
        if (current->type == BLOCK_FREE) {
            printf("Hole %d: Start = %zu KB, End = %zu KB, Size = %zu KB\n",
                   hole_num++,
                   current->start / 1024,
                   current->end / 1024,
                   current->size / 1024);
            hole_count++;
        }
        current = current->next;
    }
    if (hole_count == 0) {
        printf("(No free memory)\n");
    }
    
    // Calculate and print summary
    size_t total_allocated, total_free, largest_hole;
    double external_frag, avg_hole_size;
    
    calculate_fragmentation(alloc, &total_allocated, &total_free, &largest_hole, 
                          &external_frag, &avg_hole_size);
    
    printf("Summary:\n");
    printf("Total allocated: %zu KB\n", total_allocated / 1024);
    printf("Total free: %zu KB\n", total_free / 1024);
    printf("Largest hole: %zu KB\n", largest_hole / 1024);
    printf("External fragmentation: %.1f%% (1-largest free block/total free memory)\n", external_frag);
    printf("Average hole size: %.0f KB\n", avg_hole_size / 1024);
    
    // Print visualization if requested
    if (visualize) {
        printf("\n");
        print_visualization(alloc);
    }
}

/* ============================================================================
 * COMMAND PARSING AND EXECUTION
 * ============================================================================ */

/**
 * @brief Parse allocation strategy character
 * 
 * @param strategy_char Character representing strategy (F, B, or W)
 * @return AllocationStrategy enum value
 */
static AllocationStrategy parse_strategy(char strategy_char) {
    switch (toupper(strategy_char)) {
        case 'F':
            return STRATEGY_FIRST_FIT;
        case 'B':
            return STRATEGY_BEST_FIT;
        case 'W':
            return STRATEGY_WORST_FIT;
        default:
            return STRATEGY_FIRST_FIT; // Default to first fit
    }
}

/**
 * @brief Process a single command
 * 
 * @param alloc Pointer to Allocator structure
 * @param command Command string to process
 */
static void process_command(Allocator *alloc, const char *command) {
    char cmd_copy[MAX_LINE_LENGTH];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    trim_whitespace(cmd_copy);
    
    // Handle empty commands
    if (strlen(cmd_copy) == 0) {
        return;
    }
    
    // Parse command
    if (strncmp(cmd_copy, "RQ ", 3) == 0) {
        // Request memory: RQ <process> <size> <F|B|W>
        char process_name[MAX_PROCESS_NAME_LENGTH];
        char size_str[64];
        char strategy_char;
        
        if (sscanf(cmd_copy + 3, "%s %s %c", process_name, size_str, &strategy_char) != 3) {
            printf("Error: Invalid RQ command format. Use: RQ <process> <size> <F|B|W>\n");
            return;
        }
        
        // Validate process name
        if (!is_valid_process_name(process_name)) {
            printf("Error: Invalid process name '%s'\n", process_name);
            return;
        }
        
        // Check for duplicate process name
        MemoryBlock *current = alloc->blocks;
        while (current != NULL) {
            if (current->type == BLOCK_ALLOCATED && 
                strcmp(current->process_name, process_name) == 0) {
                printf("Error: Process '%s' already exists\n", process_name);
                return;
            }
            current = current->next;
        }
        
        size_t size = parse_size(size_str);
        if (size == 0) {
            printf("Error: Invalid size '%s'\n", size_str);
            return;
        }
        
        AllocationStrategy strategy = parse_strategy(strategy_char);
        MemoryBlock *block = allocate_memory(alloc, size, process_name, strategy);
        
        if (block == NULL) {
            printf("Error: Insufficient memory to allocate %zu bytes for process '%s'\n", size, process_name);
        } else {
            alloc->process_count++;
        }
        
    } else if (strncmp(cmd_copy, "RL ", 3) == 0) {
        // Release memory: RL <process>
        char process_name[MAX_PROCESS_NAME_LENGTH];
        
        if (sscanf(cmd_copy + 3, "%s", process_name) != 1) {
            printf("Error: Invalid RL command format. Use: RL <process>\n");
            return;
        }
        
        if (release_memory(alloc, process_name) != 0) {
            printf("Error: Process '%s' not found\n", process_name);
        }
        
    } else if (strcmp(cmd_copy, "C") == 0) {
        // Compact memory
        compact_memory(alloc);
        
    } else if (strncmp(cmd_copy, "STAT", 4) == 0) {
        // Status report: STAT or STAT -v
        bool visualize = false;
        if (strstr(cmd_copy, "-v") != NULL) {
            visualize = true;
        }
        print_statistics(alloc, visualize);
        
    } else if (strncmp(cmd_copy, "SIM ", 4) == 0) {
        // Simulation mode: SIM <filename>
        char filename[MAX_LINE_LENGTH];
        
        if (sscanf(cmd_copy + 4, "%s", filename) != 1) {
            printf("Error: Invalid SIM command format. Use: SIM <filename>\n");
            return;
        }
        
        execute_simulation(alloc, filename);
        
    } else if (strcmp(cmd_copy, "X") == 0) {
        // Exit - handled in main loop
        return;
        
    } else {
        printf("Error: Unknown command '%s'\n", cmd_copy);
        printf("Valid commands: RQ, RL, C, STAT, STAT -v, SIM, X\n");
    }
}

/**
 * @brief Execute commands from a file
 * 
 * @param alloc Pointer to Allocator structure
 * @param filename Name of the file containing commands
 */
static void execute_simulation(Allocator *alloc, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Cannot open file '%s'\n", filename);
        return;
    }
    
    char line[MAX_LINE_LENGTH];
    
    while (fgets(line, sizeof(line), file) != NULL) {
        
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Skip empty lines and comments
        trim_whitespace(line);
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // Process command
        process_command(alloc, line);
    }
    
    fclose(file);
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Parse a size string (supports KB, MB suffixes)
 * 
 * @param str String to parse
 * @return Size in bytes, or 0 on error
 */
static size_t parse_size(const char *str) {
    if (str == NULL || strlen(str) == 0) {
        return 0;
    }
    
    char *endptr;
    long long value = strtoll(str, &endptr, 10);
    
    if (value < 0) {
        return 0;
    }
    
    // Check for suffix
    while (*endptr == ' ' || *endptr == '\t') {
        endptr++;
    }
    
    if (*endptr == '\0') {
        // No suffix - assume bytes
        return (size_t)value;
    }
    
    // Check for KB, MB suffixes
    if (strcasecmp(endptr, "KB") == 0 || strcasecmp(endptr, "K") == 0) {
        return (size_t)(value * 1024);
    } else if (strcasecmp(endptr, "MB") == 0 || strcasecmp(endptr, "M") == 0) {
        return (size_t)(value * 1024 * 1024);
    } else if (strcasecmp(endptr, "GB") == 0 || strcasecmp(endptr, "G") == 0) {
        return (size_t)(value * 1024ULL * 1024 * 1024);
    }
    
    // Try to parse as plain number
    value = strtoll(str, &endptr, 10);
    if (*endptr == '\0' && value >= 0) {
        return (size_t)value;
    }
    
    return 0;
}

/**
 * @brief Trim leading and trailing whitespace from a string
 * 
 * @param str String to trim (modified in place)
 */
static void trim_whitespace(char *str) {
    if (str == NULL) {
        return;
    }
    
    // Trim leading whitespace
    char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    
    // Trim trailing whitespace
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    // Move trimmed string to beginning
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * @brief Check if a process name is valid
 * 
 * @param name Process name to check
 * @return true if valid, false otherwise
 */
static bool is_valid_process_name(const char *name) {
    if (name == NULL || strlen(name) == 0) {
        return false;
    }
    
    // Check length
    if (strlen(name) >= MAX_PROCESS_NAME_LENGTH) {
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    for (size_t i = 0; i < strlen(name); i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            return false;
        }
    }
    
    return true;
}

