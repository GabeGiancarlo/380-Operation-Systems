#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

// Technical: BUFFER_ITEM structure contains the data payload and checksum for validation.
// The checksum is calculated as the sum of all bytes in the data array, providing
// a simple integrity check mechanism. This structure is used by both producer and
// consumer threads for safe data transfer through the bounded buffer.
//
// Simple: This defines what a "package" looks like - it contains some data and a
// special number (checksum) that helps us verify the data wasn't corrupted.

#define BUFFER_SIZE 10  // Size of the circular buffer

typedef struct buffer_item {
    uint8_t data[30];   // Data payload (30 bytes)
    uint16_t cksum;     // Checksum for data integrity validation
} BUFFER_ITEM;

#endif // BUFFER_H
