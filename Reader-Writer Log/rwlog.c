#include "rw_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* Monitor state structure */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t read_cond;
    pthread_cond_t write_cond;
    
    /* Synchronization state */
    int readers_active;
    int readers_waiting;
    int writers_waiting;
    int writer_active;
    
    /* Circular buffer state */
    rwlog_entry_t *entries;
    size_t capacity;
    size_t head;        /* Next write position */
    size_t tail;        /* Oldest entry position */
    size_t count;       /* Current number of entries */
    uint64_t next_seq;  /* Next sequence number */
    
    /* Shared memory info */
    int shm_fd;
    char *shm_name;
} rwlog_monitor_t;

static rwlog_monitor_t *monitor = NULL;

/* Helper function to get current time */
static void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_REALTIME, ts);
}

/* Helper function to calculate buffer index */
static size_t buffer_index(size_t pos, size_t capacity) {
    return pos % capacity;
}

int rwlog_create(size_t capacity) {
    if (monitor != NULL) {
        fprintf(stderr, "rwlog_create: monitor already exists\n");
        return -1;
    }
    
    if (capacity == 0) {
        fprintf(stderr, "rwlog_create: capacity must be > 0\n");
        return -1;
    }
    
    /* Allocate monitor structure */
    monitor = malloc(sizeof(rwlog_monitor_t));
    if (monitor == NULL) {
        fprintf(stderr, "rwlog_create: malloc failed\n");
        return -1;
    }
    
    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&monitor->mutex, NULL) != 0) {
        fprintf(stderr, "rwlog_create: pthread_mutex_init failed\n");
        free(monitor);
        monitor = NULL;
        return -1;
    }
    
    if (pthread_cond_init(&monitor->read_cond, NULL) != 0) {
        fprintf(stderr, "rwlog_create: pthread_cond_init read_cond failed\n");
        pthread_mutex_destroy(&monitor->mutex);
        free(monitor);
        monitor = NULL;
        return -1;
    }
    
    if (pthread_cond_init(&monitor->write_cond, NULL) != 0) {
        fprintf(stderr, "rwlog_create: pthread_cond_init write_cond failed\n");
        pthread_cond_destroy(&monitor->read_cond);
        pthread_mutex_destroy(&monitor->mutex);
        free(monitor);
        monitor = NULL;
        return -1;
    }
    
    /* Initialize state */
    monitor->readers_active = 0;
    monitor->readers_waiting = 0;
    monitor->writers_waiting = 0;
    monitor->writer_active = 0;
    monitor->capacity = capacity;
    monitor->head = 0;
    monitor->tail = 0;
    monitor->count = 0;
    monitor->next_seq = 1;
    
    /* Create shared memory for the circular buffer */
    monitor->shm_name = "/rwlog_shm";
    monitor->shm_fd = shm_open(monitor->shm_name, O_CREAT | O_RDWR, 0666);
    if (monitor->shm_fd == -1) {
        fprintf(stderr, "rwlog_create: shm_open failed: %s\n", strerror(errno));
        pthread_cond_destroy(&monitor->write_cond);
        pthread_cond_destroy(&monitor->read_cond);
        pthread_mutex_destroy(&monitor->mutex);
        free(monitor);
        monitor = NULL;
        return -1;
    }
    
    /* Set size of shared memory */
    if (ftruncate(monitor->shm_fd, capacity * sizeof(rwlog_entry_t)) == -1) {
        fprintf(stderr, "rwlog_create: ftruncate failed: %s\n", strerror(errno));
        close(monitor->shm_fd);
        shm_unlink(monitor->shm_name);
        pthread_cond_destroy(&monitor->write_cond);
        pthread_cond_destroy(&monitor->read_cond);
        pthread_mutex_destroy(&monitor->mutex);
        free(monitor);
        monitor = NULL;
        return -1;
    }
    
    /* Map shared memory */
    monitor->entries = mmap(NULL, capacity * sizeof(rwlog_entry_t),
                           PROT_READ | PROT_WRITE, MAP_SHARED, monitor->shm_fd, 0);
    if (monitor->entries == MAP_FAILED) {
        fprintf(stderr, "rwlog_create: mmap failed: %s\n", strerror(errno));
        close(monitor->shm_fd);
        shm_unlink(monitor->shm_name);
        pthread_cond_destroy(&monitor->write_cond);
        pthread_cond_destroy(&monitor->read_cond);
        pthread_mutex_destroy(&monitor->mutex);
        free(monitor);
        monitor = NULL;
        return -1;
    }
    
    return 0;
}

int rwlog_destroy(void) {
    if (monitor == NULL) {
        return 0; /* Already destroyed */
    }
    
    /* Unmap shared memory */
    if (monitor->entries != NULL) {
        munmap(monitor->entries, monitor->capacity * sizeof(rwlog_entry_t));
    }
    
    /* Close and unlink shared memory */
    if (monitor->shm_fd != -1) {
        close(monitor->shm_fd);
        shm_unlink(monitor->shm_name);
    }
    
    /* Destroy synchronization primitives */
    pthread_cond_destroy(&monitor->write_cond);
    pthread_cond_destroy(&monitor->read_cond);
    pthread_mutex_destroy(&monitor->mutex);
    
    /* Free monitor structure */
    free(monitor);
    monitor = NULL;
    
    return 0;
}

int rwlog_begin_read(void) {
    if (monitor == NULL) {
        fprintf(stderr, "rwlog_begin_read: monitor not initialized\n");
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    /* Writer preference: block if writer is active or writers are waiting */
    while (monitor->writer_active || monitor->writers_waiting > 0) {
        monitor->readers_waiting++;
        pthread_cond_wait(&monitor->read_cond, &monitor->mutex);
        monitor->readers_waiting--;
    }
    
    monitor->readers_active++;
    pthread_mutex_unlock(&monitor->mutex);
    
    return 0;
}

ptrdiff_t rwlog_snapshot(rwlog_entry_t *buf, size_t max_entries) {
    if (monitor == NULL || buf == NULL) {
        return -1;
    }
    
    if (max_entries == 0) {
        return 0;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    size_t entries_to_copy = (monitor->count < max_entries) ? monitor->count : max_entries;
    
    if (entries_to_copy > 0) {
        /* Copy from tail to head, wrapping around if necessary */
        size_t start_pos = monitor->tail;
        for (size_t i = 0; i < entries_to_copy; i++) {
            size_t src_idx = buffer_index(start_pos + i, monitor->capacity);
            buf[i] = monitor->entries[src_idx];
        }
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    
    return entries_to_copy;
}

int rwlog_end_read(void) {
    if (monitor == NULL) {
        fprintf(stderr, "rwlog_end_read: monitor not initialized\n");
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    monitor->readers_active--;
    
    /* If this was the last reader and writers are waiting, signal a writer */
    if (monitor->readers_active == 0 && monitor->writers_waiting > 0) {
        pthread_cond_signal(&monitor->write_cond);
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    
    return 0;
}

int rwlog_begin_write(void) {
    if (monitor == NULL) {
        fprintf(stderr, "rwlog_begin_write: monitor not initialized\n");
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    monitor->writers_waiting++;
    
    /* Wait until no readers are active and no writer is active */
    while (monitor->readers_active > 0 || monitor->writer_active) {
        pthread_cond_wait(&monitor->write_cond, &monitor->mutex);
    }
    
    monitor->writers_waiting--;
    monitor->writer_active = 1;
    
    pthread_mutex_unlock(&monitor->mutex);
    
    return 0;
}

int rwlog_append(const rwlog_entry_t *e) {
    if (monitor == NULL || e == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    /* Add entry to circular buffer */
    size_t write_pos = monitor->head;
    monitor->entries[write_pos] = *e;
    
    /* Fill in monitor-assigned fields */
    monitor->entries[write_pos].seq = monitor->next_seq++;
    monitor->entries[write_pos].tid = pthread_self();
    get_current_time(&monitor->entries[write_pos].ts);
    
    /* Update buffer state */
    monitor->head = buffer_index(monitor->head + 1, monitor->capacity);
    
    if (monitor->count < monitor->capacity) {
        monitor->count++;
    } else {
        /* Buffer is full, advance tail to maintain sequence order */
        monitor->tail = buffer_index(monitor->tail + 1, monitor->capacity);
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    
    return 0;
}

int rwlog_end_write(void) {
    if (monitor == NULL) {
        fprintf(stderr, "rwlog_end_write: monitor not initialized\n");
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    monitor->writer_active = 0;
    
    /* Writer preference: if writers are waiting, signal one writer */
    if (monitor->writers_waiting > 0) {
        pthread_cond_signal(&monitor->write_cond);
    } else {
        /* Otherwise, broadcast to all waiting readers */
        pthread_cond_broadcast(&monitor->read_cond);
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    
    return 0;
}

void rwlog_wake_all(void) {
    if (monitor == NULL) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    pthread_cond_broadcast(&monitor->read_cond);
    pthread_cond_broadcast(&monitor->write_cond);
    pthread_mutex_unlock(&monitor->mutex);
}
