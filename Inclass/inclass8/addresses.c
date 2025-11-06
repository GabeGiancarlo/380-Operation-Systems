#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PAGE_SIZE 4096  // 4 KB = 2^12 bytes
#define OFFSET_MASK 0xFFF  // 12 bits mask (4095 in decimal)

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <virtual_address>\n", argv[0]);
        return 1;
    }

    // Parse the virtual address from command line
    unsigned long address = strtoul(argv[1], NULL, 10);
    
    // Ensure we're working with 32-bit values
    uint32_t virtual_addr = (uint32_t)address;

    // HIGH ORDER BITS (page number): Right shift drops lower bits
    // Example: 19986 >> 12 shifts right by 12, keeping only upper 20 bits
    // Bits 12-31 become bits 0-19 (the page number)
    uint32_t page_number = virtual_addr >> 12;

    // LOW ORDER BITS (offset): Bitwise AND extracts lower bits
    // 0xFFF = 0b111111111111 (12 ones) - masks out everything except bits 0-11
    // Example: 19986 & 0xFFF keeps only the lower 12 bits
    uint32_t offset = virtual_addr & OFFSET_MASK;

    // Output the results
    printf("The address %u contains:\n", virtual_addr);
    printf("        page number = %u\n", page_number);
    printf("        offset = %u\n", offset);

    return 0;
}

