# CPU Scheduling Simulator (schedsim)

A comprehensive CPU scheduling simulator that implements four scheduling algorithms using POSIX threads and semaphores. This program demonstrates how different scheduling policies affect process execution order and system performance metrics.

## Features

- **Four Scheduling Algorithms:**
  - **FCFS (First Come, First Served)** - Non-preemptive, schedules processes in arrival order
  - **SJF (Shortest Job First)** - Non-preemptive, schedules shortest remaining burst time first
  - **RR (Round Robin)** - Preemptive, cycles through processes with a fixed time quantum
  - **Priority Scheduling** - Preemptive, schedules highest priority (lowest value) first

- **Thread-Based Architecture:**
  - Each process runs in its own POSIX thread
  - Semaphores control process execution (scheduler signals when to run)
  - Main thread acts as the scheduler making all decisions

- **Comprehensive Metrics:**
  - Per-process metrics: Arrival, Start, Finish, Waiting, Response, and Turnaround times
  - Global metrics: Average waiting time, average response time, average turnaround time, throughput, and CPU utilization

- **Visual Output:**
  - Gantt chart showing process execution timeline
  - Detailed statistics table

## Requirements

- C compiler with C99 support (gcc recommended)
- POSIX threads library (pthread)
- POSIX semaphores support
- getopt_long() support (standard on Linux/macOS)

## Compilation

```bash
make
```

Or compile manually:

```bash
gcc -Wall -Wextra -std=c99 -pthread -O2 -g -o schedsim schedsim.c -lpthread
```

## Usage

```bash
./schedsim [OPTIONS]
```

### Command-Line Options

| Short | Long | Description |
|-------|------|-------------|
| `-f` | `--fcfs` | Use FCFS (First Come, First Served) scheduling |
| `-s` | `--sjf` | Use SJF (Shortest Job First) scheduling |
| `-r` | `--rr` | Use Round Robin scheduling |
| `-p` | `--priority` | Use Priority scheduling |
| `-i` | `--input <file>` | Input CSV filename (required) |
| `-q` | `--quantum <n>` | Time quantum for Round Robin (required for RR) |

### Examples

**FCFS Scheduling:**
```bash
./schedsim -f -i test_processes.csv
```

**SJF Scheduling:**
```bash
./schedsim -s -i test_processes.csv
```

**Round Robin with quantum 4:**
```bash
./schedsim -r -i test_processes.csv -q 4
```

**Priority Scheduling:**
```bash
./schedsim -p -i test_processes.csv
```

## Input File Format

The input file must be a CSV file with the following format:

```
pid,arrival,burst,priority
```

Where:
- `pid` - Process identifier (string)
- `arrival` - Arrival time in cycles (integer, >= 0)
- `burst` - Total CPU time required (integer, > 0)
- `priority` - Process priority (integer, >= 0, lower = higher priority)

### Example Input File

```
P1,0,5,3
P2,1,3,1
P3,2,8,4
P4,3,6,2
```

This defines:
- P1: arrives at time 0, needs 5 units of CPU, priority 3
- P2: arrives at time 1, needs 3 units of CPU, priority 1
- P3: arrives at time 2, needs 8 units of CPU, priority 4
- P4: arrives at time 3, needs 6 units of CPU, priority 2

## Output Format

The program produces output in the following format:

```
===== [Algorithm Name] Scheduling =====
Timeline (Gantt Chart):
0 5 8 16 22
|-----|-----|--------|------|
| P1  | P2  | P3     | P4   |
----------------------------------------

PID    Arr    Burst    Start    Finish    Wait    Resp    Turn
--------------------------------------------------------
P1     0      5        0        5         0       0       5
P2     1      3        5        8         4       4       7
P3     2      8        8        16        6       6       14
P4     3      6        16       22        13      13      19
--------------------------------------------------------
Avg Wait = 5.75
Avg Resp = 5.75
Avg Turn = 11.25
Throughput = 0.18 jobs/unit time
CPU Utilization = 100%
```

## Metrics Explained

### Per-Process Metrics

- **Arrival Time**: The simulation time when the process first enters the system
- **Start Time**: The first time the process is actually dispatched by the scheduler
- **Finish Time**: The simulation time when the process completes all its CPU bursts
- **Waiting Time**: Total time the process spends in the READY queue waiting for CPU service
- **Response Time**: Time between the process's arrival and the first time it begins execution
- **Turnaround Time**: Total time from process arrival to completion (waiting + execution time)

### Global Metrics

- **Average Waiting Time**: Average amount of time processes spend waiting in the READY queue
- **Average Response Time**: Average delay between process arrival and first execution
- **Average Turnaround Time**: Average total time from process arrival to completion
- **Throughput**: Number of processes completed per unit of simulated time
- **CPU Utilization**: Percentage of total simulated time during which the CPU was busy

## Algorithm Details

### FCFS (First Come, First Served)
- **Type**: Non-preemptive
- **Selection**: Process that arrived first (FIFO)
- **Characteristics**: Simple, fair in arrival order, but can cause long waiting times for short processes

### SJF (Shortest Job First)
- **Type**: Non-preemptive
- **Selection**: Process with smallest remaining burst time
- **Characteristics**: Minimizes average waiting time, but requires knowledge of burst times

### Round Robin
- **Type**: Preemptive
- **Selection**: Processes in cyclic order
- **Parameters**: Time quantum (q)
- **Characteristics**: Fair, prevents starvation, good for time-sharing systems

### Priority Scheduling
- **Type**: Preemptive
- **Selection**: Process with highest priority (lowest priority value)
- **Characteristics**: Can prioritize important processes, but may cause starvation

## Testing

Run the test suite:

```bash
make test
```

This will test all four scheduling algorithms with the provided test file.

## Architecture

### Thread Model

Each process is represented by a POSIX thread that:
1. Blocks on its own semaphore (initialized to 0)
2. Waits for the scheduler to signal it
3. Executes one CPU cycle when signaled
4. Decrements its remaining burst time
5. Signals completion back to the scheduler

### Scheduler Model

The main thread acts as the scheduler:
1. Maintains a READY queue of runnable processes
2. Checks for new arrivals each cycle
3. Selects next process based on scheduling algorithm
4. Signals the selected process's semaphore
5. Waits for process to complete one cycle
6. Updates metrics and Gantt chart
7. Repeats until all processes finish

### Synchronization

- **Mutex**: Protects shared scheduler state (ready queue, metrics, etc.)
- **Per-process Semaphores**: Control when each process thread executes
- **Cycle Semaphore**: Synchronizes scheduler with process threads

## Code Structure

The code is organized into the following sections:

1. **Data Structures**: Process, ReadyQueue, Scheduler, GanttEntry
2. **Command-line Parsing**: getopt_long() implementation
3. **CSV Parsing**: File input and process loading
4. **Thread Management**: Process thread creation and synchronization
5. **Queue Operations**: READY queue management (enqueue, dequeue, search)
6. **Scheduling Algorithms**: Implementation of each algorithm
7. **Scheduler Core**: Main simulation loop
8. **Output & Reporting**: Gantt chart and statistics printing

## Error Handling

The program handles various error conditions:
- Invalid command-line arguments
- Missing or invalid input file
- CSV parsing errors
- Thread creation failures
- Semaphore initialization failures
- Memory allocation failures

## Limitations

- Maximum 100 processes (configurable via MAX_PROCESSES)
- Processes must have positive burst times
- Priorities are non-negative integers
- Simulation runs until all processes complete

## Author

Gabriel Giancarlo

## License

This is an academic project for CPSC 380 - Operating Systems.

