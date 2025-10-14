
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include "rw_log.h"

struct config {
    int capacity;
    int readers;
    int writers;
    int writer_batch;
    int seconds;
    int rd_us;
    int wr_us;
    int dump_csv;
};

/* Global variables */
static volatile int stop_flag = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Statistics */
static double *writer_wait_times = NULL;
static double *reader_cs_times = NULL;
static int writer_wait_count = 0;
static int reader_cs_count = 0;
static int total_entries_written = 0;

static void print_usage(const char *progname) 
{
	fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "-c,  --capacity <N>        Log capacity (default 1024)\n"
        "-r,  --readers <N>         Number of reader threads (default 6)\n"
        "-w,  --writers <N>         Number of writer threads (default 4)\n"
        "-b,  --writer-batch <N>    Entries written per writer section (default 2)\n"
        "-s,  --seconds <N>         Total run time (default 10)\n"
        "-R,  --rd-us <usec>        Reader sleep between operations (default 2000)\n"
        "-W,  --wr-us <usec>        Writer sleep between operations (default 3000)\n"
        "-d,  --dump                Dump final log to log.csv\n"
        "-h,  --help                Show this help message\n",
        progname);
}

/* Helper function to get current time in milliseconds */
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* Signal handler for SIGINT */
static void sigint_handler(int sig) {
    (void)sig;
    stop_flag = 1;
    rwlog_wake_all();
}

/* Timer thread function */
static void *timer_thread(void *arg) {
    int seconds = *(int *)arg;
    sleep(seconds);
    stop_flag = 1;
    rwlog_wake_all();
    return NULL;
}

/* Writer thread parameters */
typedef struct {
    int writer_id;
    int batch_size;
    int sleep_us;
} writer_params_t;

/* Reader thread parameters */
typedef struct {
    int reader_id;
    int sleep_us;
} reader_params_t;

/* Writer thread function */
static void *writer_thread(void *arg) {
    writer_params_t *params = (writer_params_t *)arg;
    
    int writer_id = params->writer_id;
    int batch_size = params->batch_size;
    int sleep_us = params->sleep_us;
    int local_count = 0;
    
    while (!stop_flag) {
        double start_time = get_time_ms();
        
        if (rwlog_begin_write() != 0) {
            fprintf(stderr, "Writer %d: rwlog_begin_write failed\n", writer_id);
            break;
        }
        
        double wait_time = get_time_ms() - start_time;
        
        /* Record wait time */
        pthread_mutex_lock(&stats_mutex);
        if (writer_wait_count < 10000) { /* Reasonable limit */
            writer_wait_times[writer_wait_count++] = wait_time;
        }
        pthread_mutex_unlock(&stats_mutex);
        
        /* Append batch_size entries */
        for (int i = 0; i < batch_size && !stop_flag; i++) {
            rwlog_entry_t entry;
            snprintf(entry.msg, sizeof(entry.msg), "writer%d-msg%d", writer_id, local_count++);
            
            if (rwlog_append(&entry) != 0) {
                fprintf(stderr, "Writer %d: rwlog_append failed\n", writer_id);
                break;
            }
            
            pthread_mutex_lock(&stats_mutex);
            total_entries_written++;
            pthread_mutex_unlock(&stats_mutex);
        }
        
        if (rwlog_end_write() != 0) {
            fprintf(stderr, "Writer %d: rwlog_end_write failed\n", writer_id);
            break;
        }
        
        if (sleep_us > 0) {
            usleep(sleep_us);
        }
    }
    
    return NULL;
}

/* Reader thread function */
static void *reader_thread(void *arg) {
    reader_params_t *params = (reader_params_t *)arg;
    
    int reader_id = params->reader_id;
    int sleep_us = params->sleep_us;
    uint64_t last_seq = 0;
    
    while (!stop_flag) {
        double start_time = get_time_ms();
        
        if (rwlog_begin_read() != 0) {
            fprintf(stderr, "Reader %d: rwlog_begin_read failed\n", reader_id);
            break;
        }
        
        rwlog_entry_t buffer[128];
        ptrdiff_t count = rwlog_snapshot(buffer, 128);
        
        if (count > 0) {
            /* Check sequence monotonicity within this snapshot only */
            for (ptrdiff_t i = 1; i < count; i++) {
                if (buffer[i].seq <= buffer[i-1].seq) {
                    fprintf(stderr, "Reader %d: sequence monotonicity violation within snapshot: seq[%zd]=%llu, seq[%zd]=%llu\n", 
                            reader_id, i-1, buffer[i-1].seq, i, buffer[i].seq);
                }
            }
            /* Update last_seq to the highest sequence in this snapshot */
            if (count > 0) {
                last_seq = buffer[count-1].seq;
            }
        }
        
        if (rwlog_end_read() != 0) {
            fprintf(stderr, "Reader %d: rwlog_end_read failed\n", reader_id);
            break;
        }
        
        double cs_time = get_time_ms() - start_time;
        
        /* Record critical section time */
        pthread_mutex_lock(&stats_mutex);
        if (reader_cs_count < 10000) { /* Reasonable limit */
            reader_cs_times[reader_cs_count++] = cs_time;
        }
        pthread_mutex_unlock(&stats_mutex);
        
        if (sleep_us > 0) {
            usleep(sleep_us);
        }
    }
    
    return NULL;
}

static void parse_args(int argc, char **argv, struct config *cfg) 
{
	// Set defaults
    cfg->capacity     = 1024;
    cfg->readers      = 6;
    cfg->writers      = 4;
    cfg->writer_batch = 2;
    cfg->seconds      = 10;
    cfg->rd_us        = 2000;
    cfg->wr_us        = 3000;
    cfg->dump_csv     = 0;
	
	/* define the long_opts options */
	static struct option long_opts[] = {
        {"capacity",     required_argument, 0, 'c'},
        {"readers",      required_argument, 0, 'r'},
        {"writers",      required_argument, 0, 'w'},
        {"writer-batch", required_argument, 0, 'b'},
        {"seconds",      required_argument, 0, 's'},
        {"rd-us",        required_argument, 0, 'R'},
        {"wr-us",        required_argument, 0, 'W'},
        {"dump",         no_argument,       0, 'd'},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
	
	/* Parse each of the arguments using getopt_long() function */
	int opt;
    int opt_index;
    
    while ((opt = getopt_long(argc, argv, "c:r:w:b:s:R:W:dh", long_opts, &opt_index)) != -1) {
        switch (opt) {
            case 'c':
                cfg->capacity = atoi(optarg);
                break;
            case 'r':
                cfg->readers = atoi(optarg);
                break;
            case 'w':
                cfg->writers = atoi(optarg);
                break;
            case 'b':
                cfg->writer_batch = atoi(optarg);
                break;
            case 's':
                cfg->seconds = atoi(optarg);
                break;
            case 'R':
                cfg->rd_us = atoi(optarg);
                break;
            case 'W':
                cfg->wr_us = atoi(optarg);
                break;
            case 'd':
                cfg->dump_csv = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }
}

int main(int argc, char **argv) 
{
    struct config cfg;
    parse_args(argc, argv, &cfg);

    printf("capacity=%d readers=%d writers=%d batch=%d seconds=%d rd_us=%d wr_us=%d dump=%d\n",
           cfg.capacity, cfg.readers, cfg.writers, cfg.writer_batch,
           cfg.seconds, cfg.rd_us, cfg.wr_us, cfg.dump_csv);

    /* Allocate statistics arrays */
    writer_wait_times = malloc(10000 * sizeof(double));
    reader_cs_times = malloc(10000 * sizeof(double));
    if (writer_wait_times == NULL || reader_cs_times == NULL) {
        fprintf(stderr, "Failed to allocate statistics arrays\n");
        return 1;
    }
	
	/* Initialize the shm-backed monitor */
    if (rwlog_create(cfg.capacity) != 0) {
        fprintf(stderr, "Failed to create monitor\n");
        free(writer_wait_times);
        free(reader_cs_times);
        return 1;
    }

    /* Install SIGINT and start wall-clock timer thread */
    signal(SIGINT, sigint_handler);
    
    pthread_t timer_tid;
    if (pthread_create(&timer_tid, NULL, timer_thread, &cfg.seconds) != 0) {
        fprintf(stderr, "Failed to create timer thread\n");
        rwlog_destroy();
        free(writer_wait_times);
        free(reader_cs_times);
        return 1;
    }
	
	/* Create the writer threads */
    pthread_t *writer_threads = malloc(cfg.writers * sizeof(pthread_t));
    if (writer_threads == NULL) {
        fprintf(stderr, "Failed to allocate writer thread array\n");
        rwlog_destroy();
        free(writer_wait_times);
        free(reader_cs_times);
        return 1;
    }
    
    for (int i = 0; i < cfg.writers; i++) {
        writer_params_t *params = malloc(sizeof(writer_params_t));
        params->writer_id = i;
        params->batch_size = cfg.writer_batch;
        params->sleep_us = cfg.wr_us;
        
        if (pthread_create(&writer_threads[i], NULL, writer_thread, params) != 0) {
            fprintf(stderr, "Failed to create writer thread %d\n", i);
            rwlog_destroy();
            free(writer_wait_times);
            free(reader_cs_times);
            free(writer_threads);
            return 1;
        }
    }
	 
	/* Create the reader threads */
    pthread_t *reader_threads = malloc(cfg.readers * sizeof(pthread_t));
    if (reader_threads == NULL) {
        fprintf(stderr, "Failed to allocate reader thread array\n");
        rwlog_destroy();
        free(writer_wait_times);
        free(reader_cs_times);
        free(writer_threads);
        return 1;
    }
    
    for (int i = 0; i < cfg.readers; i++) {
        reader_params_t *params = malloc(sizeof(reader_params_t));
        params->reader_id = i;
        params->sleep_us = cfg.rd_us;
        
        if (pthread_create(&reader_threads[i], NULL, reader_thread, params) != 0) {
            fprintf(stderr, "Failed to create reader thread %d\n", i);
            rwlog_destroy();
            free(writer_wait_times);
            free(reader_cs_times);
            free(writer_threads);
            free(reader_threads);
            return 1;
        }
    }
	 
	/* Join reader/writer threads and timer thread */
    pthread_join(timer_tid, NULL);
    
    for (int i = 0; i < cfg.writers; i++) {
        pthread_join(writer_threads[i], NULL);
    }
    
    for (int i = 0; i < cfg.readers; i++) {
        pthread_join(reader_threads[i], NULL);
    }
	 
	/* Optional: dump the final log to CSV for inspection/grading. */
    if (cfg.dump_csv) {
        FILE *csv_file = fopen("log.csv", "w");
        if (csv_file != NULL) {
            fprintf(csv_file, "seq,tid,ts_sec,ts_nsec,msg\n");
            
            rwlog_begin_read();
            rwlog_entry_t buffer[cfg.capacity];
            ptrdiff_t count = rwlog_snapshot(buffer, cfg.capacity);
            
            for (ptrdiff_t i = 0; i < count; i++) {
                fprintf(csv_file, "%llu,%p,%ld,%ld,%s\n",
                        buffer[i].seq, (void*)buffer[i].tid,
                        buffer[i].ts.tv_sec, buffer[i].ts.tv_nsec,
                        buffer[i].msg);
            }
            
            rwlog_end_read();
            fclose(csv_file);
            printf("Log dumped to log.csv\n");
        }
    }
	 
	/* Compute averages only (avg reader wait, avg writer wait, avg throughput) */
    double avg_writer_wait = 0.0;
    double avg_reader_cs = 0.0;
    double throughput = 0.0;
    
    if (writer_wait_count > 0) {
        for (int i = 0; i < writer_wait_count; i++) {
            avg_writer_wait += writer_wait_times[i];
        }
        avg_writer_wait /= writer_wait_count;
    }
    
    if (reader_cs_count > 0) {
        for (int i = 0; i < reader_cs_count; i++) {
            avg_reader_cs += reader_cs_times[i];
        }
        avg_reader_cs /= reader_cs_count;
    }
    
    throughput = (double)total_entries_written / cfg.seconds;
    
    printf("\n=== Performance Metrics ===\n");
    printf("Average writer wait time: %.2f ms\n", avg_writer_wait);
    printf("Average reader critical section time: %.2f ms\n", avg_reader_cs);
    printf("Total entries written: %d\n", total_entries_written);
    printf("Throughput: %.2f entries/second\n", throughput);
	 
	/* Cleanup heap and monitor resources */
    rwlog_destroy();
    free(writer_wait_times);
    free(reader_cs_times);
    free(writer_threads);
    free(reader_threads);
	  
	return 0;
}