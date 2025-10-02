#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "buffer.h"

// Global variables for synchronization
BUFFER_ITEM buffer[BUFFER_SIZE];
int in = 0;  // Index for next insertion
int out = 0; // Index for next removal

// Synchronization primitives
pthread_mutex_t mutex;     // Protects critical sections
sem_t empty;              // Counts empty slots
sem_t full;               // Counts full slots

// Thread control
volatile int running = 1; // Flag to control thread execution

// Technical: calculate_checksum() computes the sum of all bytes in the data array
// to create a simple integrity validation mechanism. This checksum is used to
// detect data corruption during producer-consumer communication. O(n) complexity
// where n is the size of the data array.
//
// Simple: This function adds up all the numbers in the data to create a special
// "fingerprint" that helps us know if the data got messed up during transport.
uint16_t calculate_checksum(const uint8_t* data, int size) {
    uint16_t sum = 0;
    for (int i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum;
}

// Technical: insert_item() implements the producer's critical section using
// semaphore-based synchronization. It waits on 'empty' semaphore to ensure
// buffer space availability, acquires mutex for mutual exclusion, performs
// the insertion into the circular buffer, updates the 'in' pointer, and
// signals 'full' semaphore. O(1) complexity with proper synchronization.
//
// Simple: This function safely puts a new "package" into the storage shelf
// while making sure no two people put packages in at the same time.
int insert_item(BUFFER_ITEM item) {
    // Wait for empty slot
    if (sem_wait(&empty) != 0) {
        perror("sem_wait empty");
        return -1;
    }
    
    // Enter critical section
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("pthread_mutex_lock");
        sem_post(&empty); // Release the semaphore we acquired
        return -1;
    }
    
    // Insert item into buffer
    buffer[in] = item;
    in = (in + 1) % BUFFER_SIZE;
    
    // Exit critical section
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror("pthread_mutex_unlock");
        return -1;
    }
    
    // Signal that buffer has one more item
    if (sem_post(&full) != 0) {
        perror("sem_post full");
        return -1;
    }
    
    return 0;
}

// Technical: remove_item() implements the consumer's critical section using
// the same synchronization pattern as insert_item() but in reverse. It waits
// on 'full' semaphore for data availability, acquires mutex, removes item
// from circular buffer, updates 'out' pointer, and signals 'empty' semaphore.
// O(1) complexity with proper synchronization.
//
// Simple: This function safely takes a "package" from the storage shelf
// while making sure no two people take packages at the same time.
int remove_item(BUFFER_ITEM* item) {
    // Wait for available item
    if (sem_wait(&full) != 0) {
        perror("sem_wait full");
        return -1;
    }
    
    // Enter critical section
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("pthread_mutex_lock");
        sem_post(&full); // Release the semaphore we acquired
        return -1;
    }
    
    // Remove item from buffer
    *item = buffer[out];
    out = (out + 1) % BUFFER_SIZE;
    
    // Exit critical section
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror("pthread_mutex_unlock");
        return -1;
    }
    
    // Signal that buffer has one more empty slot
    if (sem_post(&empty) != 0) {
        perror("sem_post empty");
        return -1;
    }
    
    return 0;
}

// Technical: producer() function implements the producer thread logic. It generates
// random data, calculates checksum, creates BUFFER_ITEM, and inserts it into the
// bounded buffer. Uses a loop with running flag for clean termination. Each
// producer has a unique ID for debugging purposes.
//
// Simple: This function is like a worker who keeps making packages and putting
// them on the storage shelf until told to stop.
void* producer(void* param) {
    int producer_id = *(int*)param;
    BUFFER_ITEM item;
    
    printf("Producer %d starting\n", producer_id);
    
    while (running) {
        // Generate random data
        for (int i = 0; i < 30; i++) {
            item.data[i] = rand() % 256; // Random values 0-255
        }
        
        // Calculate and set checksum
        item.cksum = calculate_checksum(item.data, 30);
        
        // Insert item into buffer
        if (insert_item(item) == 0) {
            printf("Producer %d: inserted item with checksum %d\n", 
                   producer_id, item.cksum);
        } else {
            printf("Producer %d: failed to insert item\n", producer_id);
        }
        
        // Small delay to prevent overwhelming the system
        usleep(10000); // 10ms
    }
    
    printf("Producer %d terminating\n", producer_id);
    return NULL;
}

// Technical: consumer() function implements the consumer thread logic. It removes
// items from the bounded buffer, verifies checksum integrity, and reports any
// data corruption. Uses running flag for clean termination and unique ID for
// debugging. Critical for data integrity validation.
//
// Simple: This function is like a worker who keeps taking packages from the
// storage shelf and checking if they're still in good condition.
void* consumer(void* param) {
    int consumer_id = *(int*)param;
    BUFFER_ITEM item;
    
    printf("Consumer %d starting\n", consumer_id);
    
    while (running) {
        // Remove item from buffer
        if (remove_item(&item) == 0) {
            // Verify checksum
            uint16_t calculated_checksum = calculate_checksum(item.data, 30);
            
            if (calculated_checksum == item.cksum) {
                printf("Consumer %d: consumed item with checksum %d\n", 
                       consumer_id, item.cksum);
            } else {
                printf("Consumer %d: ERROR - checksum mismatch! "
                       "Expected %d, got %d\n", 
                       consumer_id, item.cksum, calculated_checksum);
                exit(EXIT_FAILURE);
            }
        } else {
            printf("Consumer %d: failed to remove item\n", consumer_id);
        }
        
        // Small delay to prevent overwhelming the system
        usleep(10000); // 10ms
    }
    
    printf("Consumer %d terminating\n", consumer_id);
    return NULL;
}

// Technical: initialize_synchronization() sets up all synchronization primitives
// with proper error handling. Initializes mutex, empty semaphore (buffer size),
// and full semaphore (0). Critical for thread safety and proper resource management.
//
// Simple: This function sets up all the "traffic lights" and "locks" that help
// our workers coordinate properly without bumping into each other.
int initialize_synchronization() {
    // Initialize mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        return -1;
    }
    
    // Initialize empty semaphore (starts with BUFFER_SIZE empty slots)
    if (sem_init(&empty, 0, BUFFER_SIZE) != 0) {
        perror("sem_init empty");
        pthread_mutex_destroy(&mutex);
        return -1;
    }
    
    // Initialize full semaphore (starts with 0 full slots)
    if (sem_init(&full, 0, 0) != 0) {
        perror("sem_init full");
        sem_destroy(&empty);
        pthread_mutex_destroy(&mutex);
        return -1;
    }
    
    return 0;
}

// Technical: cleanup_synchronization() properly destroys all synchronization
// primitives to prevent resource leaks. Must be called before program exit.
// Follows reverse order of initialization for proper cleanup.
//
// Simple: This function cleans up all the "traffic lights" and "locks" when
// we're done, so we don't leave a mess behind.
void cleanup_synchronization() {
    sem_destroy(&full);
    sem_destroy(&empty);
    pthread_mutex_destroy(&mutex);
}

// Technical: main() function implements the program entry point with command-line
// argument parsing, thread management, and proper resource cleanup. Parses delay,
// number of producers, and number of consumers. Creates and manages thread lifecycle
// with proper error handling and graceful shutdown.
//
// Simple: This is the main boss function that starts everything up, tells workers
// what to do, waits for the right amount of time, then tells everyone to stop.
int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <delay> <#producers> <#consumers>\n", argv[0]);
        fprintf(stderr, "Example: %s 5 2 3\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Parse command line arguments
    int delay = atoi(argv[1]);
    int num_producers = atoi(argv[2]);
    int num_consumers = atoi(argv[3]);
    
    // Validate arguments
    if (delay <= 0 || num_producers <= 0 || num_consumers <= 0) {
        fprintf(stderr, "Error: All arguments must be positive integers\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Starting Producer-Consumer with %d producers, %d consumers, "
           "delay %d seconds\n", num_producers, num_consumers, delay);
    
    // Initialize random seed
    srand(time(NULL));
    
    // Initialize synchronization primitives
    if (initialize_synchronization() != 0) {
        fprintf(stderr, "Failed to initialize synchronization primitives\n");
        exit(EXIT_FAILURE);
    }
    
    // Create producer threads
    pthread_t* producer_threads = malloc(num_producers * sizeof(pthread_t));
    int* producer_ids = malloc(num_producers * sizeof(int));
    
    for (int i = 0; i < num_producers; i++) {
        producer_ids[i] = i;
        if (pthread_create(&producer_threads[i], NULL, producer, &producer_ids[i]) != 0) {
            perror("pthread_create producer");
            exit(EXIT_FAILURE);
        }
    }
    
    // Create consumer threads
    pthread_t* consumer_threads = malloc(num_consumers * sizeof(pthread_t));
    int* consumer_ids = malloc(num_consumers * sizeof(int));
    
    for (int i = 0; i < num_consumers; i++) {
        consumer_ids[i] = i;
        if (pthread_create(&consumer_threads[i], NULL, consumer, &consumer_ids[i]) != 0) {
            perror("pthread_create consumer");
            exit(EXIT_FAILURE);
        }
    }
    
    // Sleep for specified delay
    printf("Sleeping for %d seconds...\n", delay);
    sleep(delay);
    
    // Signal threads to stop
    printf("Stopping all threads...\n");
    running = 0;
    
    // Wait for all producer threads to finish
    for (int i = 0; i < num_producers; i++) {
        if (pthread_join(producer_threads[i], NULL) != 0) {
            perror("pthread_join producer");
        }
    }
    
    // Wait for all consumer threads to finish
    for (int i = 0; i < num_consumers; i++) {
        if (pthread_join(consumer_threads[i], NULL) != 0) {
            perror("pthread_join consumer");
        }
    }
    
    // Cleanup
    cleanup_synchronization();
    free(producer_threads);
    free(producer_ids);
    free(consumer_threads);
    free(consumer_ids);
    
    printf("Program completed successfully\n");
    return 0;
}
