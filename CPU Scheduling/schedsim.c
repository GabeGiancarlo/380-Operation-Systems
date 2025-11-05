/**
 * @file schedsim.c
 * @brief CPU Scheduling Simulator
 * 
 * This program simulates CPU scheduling algorithms using POSIX threads and semaphores.
 * It supports four scheduling algorithms:
 * - FCFS (First Come, First Served)
 * - SJF (Shortest Job First) - non-preemptive
 * - RR (Round Robin) - preemptive with configurable time quantum
 * - Priority Scheduling - preemptive (lower number = higher priority)
 * 
 * Each process is represented by a thread that blocks on a semaphore until
 * the scheduler dispatches it. The main thread acts as the scheduler, making
 * decisions based on the selected algorithm and maintaining a READY queue.
 * 
 * @author Gabriel Giancarlo
 * @date 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>

/* ============================================================================
 * CONSTANTS AND MACROS
 * ============================================================================ */

#define MAX_PROCESSES 100
#define MAX_LINE_LENGTH 256
#define MAX_PID_LENGTH 32

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @enum SchedulingAlgorithm
 * @brief Enumeration of supported scheduling algorithms
 */
typedef enum {
    ALGORITHM_NONE = 0,
    ALGORITHM_FCFS,
    ALGORITHM_SJF,
    ALGORITHM_RR,
    ALGORITHM_PRIORITY
} SchedulingAlgorithm;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @struct Process
 * @brief Represents a process in the simulation
 * 
 * Each process has arrival time, burst time, priority, and maintains
 * state information for scheduling decisions and metrics collection.
 */
typedef struct Process {
    char pid[MAX_PID_LENGTH];      ///< Process identifier
    int arrival_time;              ///< Time when process arrives in system
    int burst_time;                ///< Total CPU time required
    int remaining_burst;           ///< Remaining CPU time (decrements during execution)
    int priority;                  ///< Process priority (lower = higher priority)
    
    // Metrics
    int start_time;                ///< First time process is dispatched (-1 if not started)
    int finish_time;               ///< Time when process completes (-1 if not finished)
    int waiting_time;              ///< Total time waiting in READY queue
    int response_time;             ///< Time from arrival to first execution
    
    // Thread synchronization
    pthread_t thread;              ///< POSIX thread representing this process
    pthread_cond_t cond;           ///< Condition variable for scheduler control
    pthread_mutex_t cond_mutex;    ///< Mutex for condition variable
    bool should_run;               ///< Flag indicating process should execute
    
    // State flags
    bool has_arrived;              ///< Whether process has entered the system
    bool is_finished;              ///< Whether process has completed execution
    bool is_running;               ///< Whether process is currently executing
} Process;

/**
 * @struct ProcessNode
 * @brief Node in the READY queue linked list
 */
typedef struct ProcessNode {
    Process *process;              ///< Pointer to the process
    struct ProcessNode *next;      ///< Pointer to next node in queue
} ProcessNode;

/**
 * @struct ReadyQueue
 * @brief Queue structure for managing ready processes
 */
typedef struct ReadyQueue {
    ProcessNode *head;             ///< Head of the queue
    ProcessNode *tail;             ///< Tail of the queue
    int count;                     ///< Number of processes in queue
} ReadyQueue;

/**
 * @struct GanttEntry
 * @brief Entry in the Gantt chart timeline
 */
typedef struct GanttEntry {
    int start_time;                     ///< Start time of this execution
    int end_time;                       ///< End time of this execution
    char pid[MAX_PID_LENGTH];          ///< Process ID executing during this period
} GanttEntry;

/**
 * @struct Scheduler
 * @brief Main scheduler state and control structure
 */
typedef struct Scheduler {
    Process processes[MAX_PROCESSES];  ///< Array of all processes
    int process_count;                  ///< Number of processes in system
    ReadyQueue ready_queue;             ///< Queue of ready processes
    SchedulingAlgorithm algorithm;      ///< Current scheduling algorithm
    int time_quantum;                   ///< Time quantum for Round Robin
    int current_time;                   ///< Simulation clock (current cycle)
    Process *running_process;           ///< Currently executing process (NULL if idle)
    int quantum_remaining;              ///< Remaining quantum for current process
    bool all_finished;                  ///< Flag indicating all processes completed
    
    // Gantt chart tracking
    GanttEntry gantt[MAX_PROCESSES * 20]; ///< Gantt chart entries
    int gantt_count;                    ///< Number of Gantt entries
    
    // Synchronization
    pthread_mutex_t mutex;              ///< Mutex for protecting shared state
    pthread_cond_t cycle_cond;         ///< Condition variable to synchronize cycle completion
    bool cycle_complete;               ///< Flag indicating cycle completion
    
    // Statistics
    int total_busy_time;                ///< Total time CPU was busy
    int total_idle_time;                ///< Total time CPU was idle
} Scheduler;

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

static Scheduler g_scheduler;     ///< Global scheduler instance

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

// Command-line parsing
static void parse_arguments(int argc, char *argv[], char **input_file, 
                           SchedulingAlgorithm *algorithm, int *quantum);
static void print_usage(const char *program_name);

// CSV parsing
static int parse_csv_file(const char *filename, Process *processes, int max_processes);
static int parse_process_line(const char *line, Process *process);

// Process and thread management
static void *process_thread(void *arg);
static void initialize_processes(Process *processes, int count);
static void cleanup_processes(Process *processes, int count);

// READY queue operations
static void ready_queue_init(ReadyQueue *queue);
static void ready_queue_destroy(ReadyQueue *queue);
static void ready_queue_enqueue(ReadyQueue *queue, Process *process);
static Process *ready_queue_dequeue(ReadyQueue *queue);

// Scheduling algorithms
static Process *schedule_fcfs(ReadyQueue *queue);
static Process *schedule_sjf(ReadyQueue *queue);
static Process *schedule_priority(ReadyQueue *queue);

// Queue utilities for SJF and Priority
static ProcessNode *ready_queue_find_min_burst(ReadyQueue *queue);
static ProcessNode *ready_queue_find_min_priority(ReadyQueue *queue);
static void ready_queue_remove(ReadyQueue *queue, ProcessNode *node);

// Scheduler core
static void scheduler_init(Scheduler *sched, SchedulingAlgorithm alg, int quantum);
static void scheduler_run(Scheduler *sched);
static void scheduler_cleanup(Scheduler *sched);
static void check_arrivals(Scheduler *sched, int current_time);
static void dispatch_process(Scheduler *sched, Process *process);
// Output and reporting
static void print_gantt_chart(Scheduler *sched);
static void print_statistics(Scheduler *sched);
static void print_algorithm_name(SchedulingAlgorithm alg);

/* ============================================================================
 * MAIN FUNCTION
 * ============================================================================ */

/**
 * @brief Main entry point of the program
 * 
 * Parses command-line arguments, loads processes from CSV file,
 * initializes the scheduler, and runs the simulation.
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc, char *argv[]) {
    char *input_file = NULL;
    SchedulingAlgorithm algorithm = ALGORITHM_NONE;
    int quantum = 1;
    
    // Parse command-line arguments
    parse_arguments(argc, argv, &input_file, &algorithm, &quantum);
    
    // Validate that an algorithm was selected
    if (algorithm == ALGORITHM_NONE) {
        fprintf(stderr, "Error: Must specify a scheduling algorithm\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Validate input file was provided
    if (input_file == NULL) {
        fprintf(stderr, "Error: Must specify an input CSV file\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Validate quantum for Round Robin
    if (algorithm == ALGORITHM_RR && quantum <= 0) {
        fprintf(stderr, "Error: Time quantum must be positive for Round Robin\n");
        return EXIT_FAILURE;
    }
    
    // Initialize scheduler
    scheduler_init(&g_scheduler, algorithm, quantum);
    
    // Load processes from CSV file
    int process_count = parse_csv_file(input_file, g_scheduler.processes, MAX_PROCESSES);
    if (process_count <= 0) {
        fprintf(stderr, "Error: Failed to load processes from '%s'\n", input_file);
        scheduler_cleanup(&g_scheduler);
        return EXIT_FAILURE;
    }
    
    g_scheduler.process_count = process_count;
    
    // Initialize process threads and semaphores
    initialize_processes(g_scheduler.processes, process_count);
    
    // Run the simulation
    scheduler_run(&g_scheduler);
    
    // Print results
    print_algorithm_name(algorithm);
    print_gantt_chart(&g_scheduler);
    print_statistics(&g_scheduler);
    
    // Cleanup
    cleanup_processes(g_scheduler.processes, process_count);
    scheduler_cleanup(&g_scheduler);
    
    return EXIT_SUCCESS;
}

/* ============================================================================
 * COMMAND-LINE PARSING
 * ============================================================================ */

/**
 * @brief Parse command-line arguments using getopt_long
 * 
 * Supports both short (-f, -s, -r, -p, -i, -q) and long (--fcfs, --sjf, etc.)
 * option formats as required by the assignment.
 * 
 * @param argc Number of arguments
 * @param argv Argument array
 * @param input_file Output parameter for input filename
 * @param algorithm Output parameter for selected algorithm
 * @param quantum Output parameter for time quantum (for RR)
 */
static void parse_arguments(int argc, char *argv[], char **input_file,
                           SchedulingAlgorithm *algorithm, int *quantum) {
    static struct option long_options[] = {
        {"fcfs", no_argument, 0, 'f'},
        {"sjf", no_argument, 0, 's'},
        {"rr", no_argument, 0, 'r'},
        {"priority", no_argument, 0, 'p'},
        {"input", required_argument, 0, 'i'},
        {"quantum", required_argument, 0, 'q'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "fsrpi:q:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'f':
                *algorithm = ALGORITHM_FCFS;
                break;
            case 's':
                *algorithm = ALGORITHM_SJF;
                break;
            case 'r':
                *algorithm = ALGORITHM_RR;
                break;
            case 'p':
                *algorithm = ALGORITHM_PRIORITY;
                break;
            case 'i':
                *input_file = optarg;
                break;
            case 'q':
                *quantum = atoi(optarg);
                if (*quantum <= 0) {
                    fprintf(stderr, "Warning: Invalid quantum value '%s', using default 1\n", optarg);
                    *quantum = 1;
                }
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    // If quantum not specified but RR is selected, use default
    if (*algorithm == ALGORITHM_RR && *quantum == 1 && optind < argc) {
        // Check if quantum was actually provided
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quantum") == 0) {
                if (i + 1 < argc) {
                    *quantum = atoi(argv[i + 1]);
                    if (*quantum <= 0) *quantum = 1;
                }
                break;
            }
        }
    }
}

/**
 * @brief Print usage information
 * 
 * @param program_name Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f, --fcfs              Use FCFS (First Come, First Served) scheduling\n");
    fprintf(stderr, "  -s, --sjf               Use SJF (Shortest Job First) scheduling\n");
    fprintf(stderr, "  -r, --rr                Use Round Robin scheduling\n");
    fprintf(stderr, "  -p, --priority          Use Priority scheduling\n");
    fprintf(stderr, "  -i, --input <file>      Input CSV filename (required)\n");
    fprintf(stderr, "  -q, --quantum <n>       Time quantum for Round Robin (required for RR)\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s -f -i processes.csv\n", program_name);
    fprintf(stderr, "  %s -r -i processes.csv -q 4\n", program_name);
}

/* ============================================================================
 * CSV PARSING
 * ============================================================================ */

/**
 * @brief Parse a CSV file containing process information
 * 
 * Expected format: pid,arrival,burst,priority
 * Each line represents one process.
 * 
 * @param filename Path to the CSV file
 * @param processes Array to store parsed processes
 * @param max_processes Maximum number of processes to read
 * @return Number of processes successfully parsed, or -1 on error
 */
static int parse_csv_file(const char *filename, Process *processes, int max_processes) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening input file");
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    int count = 0;
    int line_num = 0;
    
    while (fgets(line, sizeof(line), file) != NULL && count < max_processes) {
        line_num++;
        
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Parse the line
        if (parse_process_line(line, &processes[count]) == 0) {
            count++;
        } else {
            fprintf(stderr, "Warning: Failed to parse line %d: %s\n", line_num, line);
        }
    }
    
    fclose(file);
    return count;
}

/**
 * @brief Parse a single line from the CSV file
 * 
 * Expected format: pid,arrival,burst,priority
 * 
 * @param line Input line string
 * @param process Process structure to populate
 * @return 0 on success, -1 on error
 */
static int parse_process_line(const char *line, Process *process) {
    // Initialize process structure
    memset(process, 0, sizeof(Process));
    process->start_time = -1;
    process->finish_time = -1;
    process->has_arrived = false;
    process->is_finished = false;
    process->is_running = false;
    
    // Parse CSV line: pid,arrival,burst,priority
    char pid[MAX_PID_LENGTH];
    int arrival, burst, priority;
    
    if (sscanf(line, "%[^,],%d,%d,%d", pid, &arrival, &burst, &priority) != 4) {
        return -1;
    }
    
    // Validate values
    if (arrival < 0 || burst <= 0 || priority < 0) {
        return -1;
    }
    
    // Copy values to process structure
    strncpy(process->pid, pid, MAX_PID_LENGTH - 1);
    process->pid[MAX_PID_LENGTH - 1] = '\0';
    process->arrival_time = arrival;
    process->burst_time = burst;
    process->remaining_burst = burst;
    process->priority = priority;
    
    return 0;
}

/* ============================================================================
 * PROCESS AND THREAD MANAGEMENT
 * ============================================================================ */

/**
 * @brief Initialize all process threads and condition variables
 * 
 * Each process gets its own thread that blocks on a condition variable.
 * The thread will remain blocked until the scheduler signals it.
 * 
 * @param processes Array of processes to initialize
 * @param count Number of processes
 */
static void initialize_processes(Process *processes, int count) {
    for (int i = 0; i < count; i++) {
        Process *proc = &processes[i];
        
        // Initialize condition variable and mutex
        if (pthread_cond_init(&proc->cond, NULL) != 0) {
            perror("Error initializing condition variable");
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_init(&proc->cond_mutex, NULL) != 0) {
            perror("Error initializing condition mutex");
            exit(EXIT_FAILURE);
        }
        proc->should_run = false;
        
        // Create thread for this process
        if (pthread_create(&proc->thread, NULL, process_thread, proc) != 0) {
            perror("Error creating thread");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * @brief Cleanup all process threads and condition variables
 * 
 * Joins all threads and destroys condition variables to free resources.
 * 
 * @param processes Array of processes to cleanup
 * @param count Number of processes
 */
static void cleanup_processes(Process *processes, int count) {
    for (int i = 0; i < count; i++) {
        Process *proc = &processes[i];
        
        // Signal thread to finish
        proc->is_finished = true;
        pthread_mutex_lock(&proc->cond_mutex);
        proc->should_run = true;
        pthread_cond_signal(&proc->cond);
        pthread_mutex_unlock(&proc->cond_mutex);
        
        // Wait for thread to complete
        pthread_join(proc->thread, NULL);
        
        // Destroy condition variable and mutex
        pthread_cond_destroy(&proc->cond);
        pthread_mutex_destroy(&proc->cond_mutex);
    }
}

/**
 * @brief Thread function representing a process
 * 
 * This function runs in a separate thread for each process. It blocks on
 * the process's semaphore until the scheduler signals it. When signaled,
 * it executes one unit of CPU time (decrements remaining_burst) and then
 * signals back to the scheduler that the cycle is complete.
 * 
 * @param arg Pointer to the Process structure
 * @return NULL (thread exit value)
 */
static void *process_thread(void *arg) {
    Process *proc = (Process *)arg;
    
    // Wait for scheduler to signal this process
    while (!proc->is_finished) {
        // Wait on condition variable until scheduler dispatches us
        pthread_mutex_lock(&proc->cond_mutex);
        while (!proc->should_run && !proc->is_finished) {
            pthread_cond_wait(&proc->cond, &proc->cond_mutex);
        }
        
        if (proc->is_finished) {
            pthread_mutex_unlock(&proc->cond_mutex);
            break;
        }
        
        proc->should_run = false;
        pthread_mutex_unlock(&proc->cond_mutex);
        
        // Execute one unit of CPU time
        pthread_mutex_lock(&g_scheduler.mutex);
        
        if (proc->remaining_burst > 0) {
            proc->remaining_burst--;
            
            // Update metrics on first dispatch
            if (proc->start_time == -1) {
                proc->start_time = g_scheduler.current_time;
                proc->response_time = proc->start_time - proc->arrival_time;
            }
            
            // Check if process is finished
            if (proc->remaining_burst == 0) {
                proc->finish_time = g_scheduler.current_time + 1;
                proc->is_finished = true;
                proc->is_running = false;
            }
        }
        
        // Signal scheduler that we've completed one cycle
        g_scheduler.cycle_complete = true;
        pthread_cond_signal(&g_scheduler.cycle_cond);
        pthread_mutex_unlock(&g_scheduler.mutex);
    }
    
    return NULL;
}

/* ============================================================================
 * READY QUEUE OPERATIONS
 * ============================================================================ */

/**
 * @brief Initialize an empty READY queue
 * 
 * @param queue Pointer to ReadyQueue structure to initialize
 */
static void ready_queue_init(ReadyQueue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

/**
 * @brief Destroy a READY queue and free all nodes
 * 
 * @param queue Pointer to ReadyQueue structure to destroy
 */
static void ready_queue_destroy(ReadyQueue *queue) {
    ProcessNode *current = queue->head;
    while (current != NULL) {
        ProcessNode *next = current->next;
        free(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

/**
 * @brief Add a process to the end of the READY queue (FIFO)
 * 
 * @param queue Pointer to ReadyQueue structure
 * @param process Pointer to Process to enqueue
 */
static void ready_queue_enqueue(ReadyQueue *queue, Process *process) {
    ProcessNode *node = (ProcessNode *)malloc(sizeof(ProcessNode));
    if (node == NULL) {
        perror("Error allocating queue node");
        exit(EXIT_FAILURE);
    }
    
    node->process = process;
    node->next = NULL;
    
    if (queue->tail == NULL) {
        // Empty queue
        queue->head = node;
        queue->tail = node;
    } else {
        // Add to tail
        queue->tail->next = node;
        queue->tail = node;
    }
    
    queue->count++;
}

/**
 * @brief Remove and return the first process from the READY queue
 * 
 * @param queue Pointer to ReadyQueue structure
 * @return Pointer to Process that was dequeued, or NULL if queue is empty
 */
static Process *ready_queue_dequeue(ReadyQueue *queue) {
    if (queue->head == NULL) {
        return NULL;
    }
    
    ProcessNode *node = queue->head;
    Process *process = node->process;
    
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    
    free(node);
    queue->count--;
    
    return process;
}


/**
 * @brief Find the process node with minimum burst time (for SJF)
 * 
 * @param queue Pointer to ReadyQueue structure
 * @return Pointer to ProcessNode with minimum burst, or NULL if queue is empty
 */
static ProcessNode *ready_queue_find_min_burst(ReadyQueue *queue) {
    if (queue->head == NULL) {
        return NULL;
    }
    
    ProcessNode *min_node = queue->head;
    ProcessNode *current = queue->head->next;
    
    while (current != NULL) {
        if (current->process->remaining_burst < min_node->process->remaining_burst) {
            min_node = current;
        }
        current = current->next;
    }
    
    return min_node;
}

/**
 * @brief Find the process node with minimum priority value (for Priority scheduling)
 * 
 * @param queue Pointer to ReadyQueue structure
 * @return Pointer to ProcessNode with minimum priority, or NULL if queue is empty
 */
static ProcessNode *ready_queue_find_min_priority(ReadyQueue *queue) {
    if (queue->head == NULL) {
        return NULL;
    }
    
    ProcessNode *min_node = queue->head;
    ProcessNode *current = queue->head->next;
    
    while (current != NULL) {
        if (current->process->priority < min_node->process->priority) {
            min_node = current;
        }
        current = current->next;
    }
    
    return min_node;
}

/**
 * @brief Remove a specific node from the READY queue
 * 
 * @param queue Pointer to ReadyQueue structure
 * @param node Pointer to ProcessNode to remove
 */
static void ready_queue_remove(ReadyQueue *queue, ProcessNode *node) {
    if (node == NULL || queue->head == NULL) {
        return;
    }
    
    // If node is the head
    if (queue->head == node) {
        queue->head = node->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        free(node);
        queue->count--;
        return;
    }
    
    // Find previous node
    ProcessNode *prev = queue->head;
    while (prev != NULL && prev->next != node) {
        prev = prev->next;
    }
    
    if (prev != NULL) {
        prev->next = node->next;
        if (queue->tail == node) {
            queue->tail = prev;
        }
        free(node);
        queue->count--;
    }
}

/* ============================================================================
 * SCHEDULING ALGORITHMS
 * ============================================================================ */

/**
 * @brief FCFS (First Come, First Served) scheduling
 * 
 * Selects the process that arrived first (FIFO order).
 * 
 * @param queue Pointer to READY queue
 * @return Pointer to Process to schedule, or NULL if queue is empty
 */
static Process *schedule_fcfs(ReadyQueue *queue) {
    return ready_queue_dequeue(queue);
}

/**
 * @brief SJF (Shortest Job First) scheduling
 * 
 * Selects the process with the smallest remaining burst time.
 * Non-preemptive: once a process starts, it runs to completion.
 * 
 * @param queue Pointer to READY queue
 * @return Pointer to Process to schedule, or NULL if queue is empty
 */
static Process *schedule_sjf(ReadyQueue *queue) {
    ProcessNode *min_node = ready_queue_find_min_burst(queue);
    if (min_node == NULL) {
        return NULL;
    }
    
    Process *selected = min_node->process;
    ready_queue_remove(queue, min_node);
    
    return selected;
}


/**
 * @brief Priority scheduling
 * 
 * Selects the process with the highest priority (lowest priority value).
 * Preemptive: higher priority process can preempt lower priority one.
 * 
 * @param queue Pointer to READY queue
 * @return Pointer to Process to schedule, or NULL if queue is empty
 */
static Process *schedule_priority(ReadyQueue *queue) {
    ProcessNode *min_node = ready_queue_find_min_priority(queue);
    if (min_node == NULL) {
        return NULL;
    }
    
    Process *selected = min_node->process;
    ready_queue_remove(queue, min_node);
    
    return selected;
}

/* ============================================================================
 * SCHEDULER CORE
 * ============================================================================ */

/**
 * @brief Initialize the scheduler
 * 
 * Sets up the scheduler with the specified algorithm and parameters.
 * 
 * @param sched Pointer to Scheduler structure
 * @param alg Scheduling algorithm to use
 * @param quantum Time quantum for Round Robin
 */
static void scheduler_init(Scheduler *sched, SchedulingAlgorithm alg, int quantum) {
    memset(sched, 0, sizeof(Scheduler));
    sched->algorithm = alg;
    sched->time_quantum = quantum;
    sched->current_time = 0;
    sched->running_process = NULL;
    sched->quantum_remaining = 0;
    sched->all_finished = false;
    sched->total_busy_time = 0;
    sched->total_idle_time = 0;
    sched->gantt_count = 0;
    
    ready_queue_init(&sched->ready_queue);
    
    if (pthread_mutex_init(&sched->mutex, NULL) != 0) {
        perror("Error initializing mutex");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_cond_init(&sched->cycle_cond, NULL) != 0) {
        perror("Error initializing cycle condition variable");
        exit(EXIT_FAILURE);
    }
    sched->cycle_complete = false;
}

/**
 * @brief Cleanup scheduler resources
 * 
 * @param sched Pointer to Scheduler structure
 */
static void scheduler_cleanup(Scheduler *sched) {
    ready_queue_destroy(&sched->ready_queue);
    pthread_mutex_destroy(&sched->mutex);
    pthread_cond_destroy(&sched->cycle_cond);
}

/**
 * @brief Main scheduling loop
 * 
 * This is the core of the scheduler. It runs the simulation clock and
 * makes scheduling decisions each cycle based on the selected algorithm.
 * 
 * @param sched Pointer to Scheduler structure
 */
static void scheduler_run(Scheduler *sched) {
    int last_gantt_end = 0;
    Process *last_gantt_process = NULL;
    
    pthread_mutex_lock(&sched->mutex);
    while (!sched->all_finished) {
        
        // Check for new arrivals
        check_arrivals(sched, sched->current_time);
        
        // Handle finished running process
        if (sched->running_process != NULL && sched->running_process->is_finished) {
            sched->running_process->is_running = false;
            sched->running_process = NULL;
            sched->quantum_remaining = 0;
        }
        
        // Select next process based on scheduling algorithm
        Process *next_process = NULL;
        bool process_changed = false;
        
        switch (sched->algorithm) {
            case ALGORITHM_FCFS:
                // Non-preemptive: only schedule if nothing is running
                if (sched->running_process == NULL) {
                    next_process = schedule_fcfs(&sched->ready_queue);
                    process_changed = (next_process != NULL);
                } else {
                    next_process = sched->running_process;
                }
                break;
                
            case ALGORITHM_SJF:
                // Non-preemptive: only schedule if nothing is running
                if (sched->running_process == NULL) {
                    next_process = schedule_sjf(&sched->ready_queue);
                    process_changed = (next_process != NULL);
                } else {
                    next_process = sched->running_process;
                }
                break;
                
            case ALGORITHM_RR:
                // Preemptive: check quantum expiration
                if (sched->quantum_remaining <= 0 && sched->running_process != NULL && 
                    !sched->running_process->is_finished) {
                    // Quantum expired, re-queue current process
                    ready_queue_enqueue(&sched->ready_queue, sched->running_process);
                    sched->running_process->is_running = false;
                    sched->running_process = NULL;
                }
                
                // Select next process
                if (sched->running_process == NULL) {
                    next_process = ready_queue_dequeue(&sched->ready_queue);
                    process_changed = (next_process != NULL);
                    if (next_process != NULL) {
                        sched->quantum_remaining = sched->time_quantum;
                    }
                } else {
                    next_process = sched->running_process;
                    sched->quantum_remaining--;
                }
                break;
                
            case ALGORITHM_PRIORITY:
                // Preemptive: check if higher priority process arrived
                if (sched->running_process != NULL && !sched->running_process->is_finished) {
                    ProcessNode *higher = ready_queue_find_min_priority(&sched->ready_queue);
                    if (higher != NULL && higher->process->priority < sched->running_process->priority) {
                        // Preempt current process
                        ready_queue_enqueue(&sched->ready_queue, sched->running_process);
                        sched->running_process->is_running = false;
                        sched->running_process = NULL;
                        next_process = schedule_priority(&sched->ready_queue);
                        process_changed = (next_process != NULL);
                    } else {
                        // Continue running current process
                        next_process = sched->running_process;
                    }
                } else {
                    next_process = schedule_priority(&sched->ready_queue);
                    process_changed = (next_process != NULL);
                }
                break;
                
            default:
                break;
        }
        
        // Update Gantt chart when process changes
        if (process_changed && next_process != NULL) {
            // Close previous Gantt entry
            if (last_gantt_process != NULL && sched->gantt_count > 0) {
                sched->gantt[sched->gantt_count - 1].end_time = sched->current_time;
            }
            
            // Add idle time if there was a gap
            if (last_gantt_end < sched->current_time && sched->gantt_count > 0) {
                // Already handled by closing previous entry
            }
            
            // Start new Gantt entry
            if (sched->gantt_count < MAX_PROCESSES * 20) {
                strcpy(sched->gantt[sched->gantt_count].pid, next_process->pid);
                sched->gantt[sched->gantt_count].start_time = sched->current_time;
                sched->gantt[sched->gantt_count].end_time = sched->current_time + 1;
                sched->gantt_count++;
            }
            
            last_gantt_process = next_process;
            last_gantt_end = sched->current_time;
        } else if (next_process != NULL && sched->gantt_count > 0) {
            // Extend current Gantt entry
            sched->gantt[sched->gantt_count - 1].end_time = sched->current_time + 1;
        }
        
        // Dispatch the selected process
        if (next_process != NULL && !next_process->is_finished) {
            // For non-preemptive algorithms, if process is already running, re-dispatch it
            // For preemptive algorithms, we might need to re-dispatch
            if (process_changed || next_process != sched->running_process) {
                // New process being dispatched
                dispatch_process(sched, next_process);
                sched->running_process = next_process;
            } else {
                // Same process continuing - re-dispatch it for next cycle
                dispatch_process(sched, next_process);
            }
            
            // Wait for process to complete one cycle
            // Note: mutex is already locked, cond_wait will atomically release it
            sched->cycle_complete = false;
            while (!sched->cycle_complete && !next_process->is_finished) {
                pthread_cond_wait(&sched->cycle_cond, &sched->mutex);
            }
            
            // Execute one cycle - process has completed it
            sched->total_busy_time++;
            
            // Update waiting time for processes in ready queue
            ProcessNode *node = sched->ready_queue.head;
            while (node != NULL) {
                if (!node->process->is_finished && node->process != sched->running_process) {
                    node->process->waiting_time++;
                }
                node = node->next;
            }
        } else {
            // CPU is idle
            sched->total_idle_time++;
        }
        
        // Advance simulation clock
        sched->current_time++;
        
        // Check if all processes are finished
        sched->all_finished = true;
        for (int i = 0; i < sched->process_count; i++) {
            if (!sched->processes[i].is_finished) {
                sched->all_finished = false;
                break;
            }
        }
        
        // Small yield to allow thread context switch
        usleep(100);
    }
    
    pthread_mutex_unlock(&sched->mutex);
    
    // Close final Gantt entry
    if (sched->gantt_count > 0) {
        sched->gantt[sched->gantt_count - 1].end_time = sched->current_time;
    }
}

/**
 * @brief Check for processes that should arrive at the current time
 * 
 * @param sched Pointer to Scheduler structure
 * @param current_time Current simulation time
 */
static void check_arrivals(Scheduler *sched, int current_time) {
    for (int i = 0; i < sched->process_count; i++) {
        Process *proc = &sched->processes[i];
        
        if (!proc->has_arrived && proc->arrival_time <= current_time) {
            proc->has_arrived = true;
            ready_queue_enqueue(&sched->ready_queue, proc);
        }
    }
}

/**
 * @brief Dispatch a process by signaling its condition variable
 * 
 * @param sched Pointer to Scheduler structure
 * @param process Pointer to Process to dispatch
 */
static void dispatch_process(Scheduler *sched, Process *process) {
    (void)sched; // Unused parameter
    process->is_running = true;
    pthread_mutex_lock(&process->cond_mutex);
    process->should_run = true;
    pthread_cond_signal(&process->cond);
    pthread_mutex_unlock(&process->cond_mutex);
}


/* ============================================================================
 * OUTPUT AND REPORTING
 * ============================================================================ */

/**
 * @brief Print the algorithm name header
 * 
 * @param alg Scheduling algorithm
 */
static void print_algorithm_name(SchedulingAlgorithm alg) {
    printf("===== ");
    switch (alg) {
        case ALGORITHM_FCFS:
            printf("FCFS Scheduling");
            break;
        case ALGORITHM_SJF:
            printf("SJF Scheduling");
            break;
        case ALGORITHM_RR:
            printf("Round Robin Scheduling");
            break;
        case ALGORITHM_PRIORITY:
            printf("Priority Scheduling");
            break;
        default:
            printf("Unknown Scheduling");
            break;
    }
    printf(" =====\n");
}


/**
 * @brief Print Gantt chart showing process execution timeline
 * 
 * @param sched Pointer to Scheduler structure
 */
static void print_gantt_chart(Scheduler *sched) {
    printf("Timeline (Gantt Chart):\n");
    
    if (sched->gantt_count == 0) {
        printf("(No processes executed)\n\n");
        return;
    }
    
    // Print timeline markers
    printf("%d", sched->gantt[0].start_time);
    for (int i = 0; i < sched->gantt_count; i++) {
        printf(" %d", sched->gantt[i].end_time);
    }
    printf("\n");
    
    // Print top border with pipes and dashes
    printf("|");
    for (int i = 0; i < sched->gantt_count; i++) {
        int duration = sched->gantt[i].end_time - sched->gantt[i].start_time;
        for (int j = 0; j < duration; j++) {
            printf("-");
        }
        printf("|");
    }
    printf("\n");
    
    // Print process labels row
    printf("|");
    for (int i = 0; i < sched->gantt_count; i++) {
        int duration = sched->gantt[i].end_time - sched->gantt[i].start_time;
        int pid_len = strlen(sched->gantt[i].pid);
        
        // Calculate spacing to center the process ID
        int total_spaces = duration - pid_len - 2; // -2 for spaces around PID
        int spaces_before = total_spaces / 2;
        int spaces_after = total_spaces - spaces_before;
        
        // Print spaces before
        for (int j = 0; j < spaces_before; j++) {
            printf(" ");
        }
        // Print process ID with spaces
        printf(" %s ", sched->gantt[i].pid);
        // Print spaces after
        for (int j = 0; j < spaces_after; j++) {
            printf(" ");
        }
        printf("|");
    }
    printf("\n");
    printf("-------------------------------------\n");
}

/**
 * @brief Print detailed statistics for all processes and overall metrics
 * 
 * @param sched Pointer to Scheduler structure
 */
static void print_statistics(Scheduler *sched) {
    printf("PID\tArr\tBurst\tStart\tFinish\tWait\tResp\tTurn\n");
    printf("--------------------------------------------------------\n");
    
    double total_wait = 0.0;
    double total_resp = 0.0;
    double total_turn = 0.0;
    
    for (int i = 0; i < sched->process_count; i++) {
        Process *proc = &sched->processes[i];
        
        int turnaround = proc->finish_time - proc->arrival_time;
        int waiting = proc->finish_time - proc->arrival_time - proc->burst_time;
        int response = proc->response_time;
        
        // Ensure non-negative values
        if (waiting < 0) waiting = 0;
        if (response < 0) response = 0;
        
        total_wait += waiting;
        total_resp += response;
        total_turn += turnaround;
        
        printf("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
               proc->pid,
               proc->arrival_time,
               proc->burst_time,
               proc->start_time,
               proc->finish_time,
               waiting,
               response,
               turnaround);
    }
    
    printf("--------------------------------------------------------\n");
    
    int n = sched->process_count;
    double avg_wait = (n > 0) ? total_wait / n : 0.0;
    double avg_resp = (n > 0) ? total_resp / n : 0.0;
    double avg_turn = (n > 0) ? total_turn / n : 0.0;
    
    int total_time = sched->current_time;
    double throughput = (total_time > 0) ? (double)n / total_time : 0.0;
    double cpu_util = (total_time > 0) ? 
        (double)sched->total_busy_time / total_time * 100.0 : 0.0;
    
    printf("Avg Wait = %.2f\n", avg_wait);
    printf("Avg Resp = %.2f\n", avg_resp);
    printf("Avg Turn = %.2f\n", avg_turn);
    printf("Throughput = %.2f jobs/unit time\n", throughput);
    printf("CPU Utilization = %.0f%%\n", cpu_util);
}

