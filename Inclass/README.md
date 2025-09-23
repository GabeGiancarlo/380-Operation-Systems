# Multithreaded Statistics Calculator

This program calculates the **average**, **minimum**, and **maximum** of a list of integers using three worker threads.  
Each value is stored in global variables and displayed after all threads finish.

## Requirements
- A C compiler (e.g., `gcc`)
- POSIX threads library (`pthread`)

## Compilation
```bash
gcc -pthread stats.c -o stats
