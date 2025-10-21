/*
 * Simple Shell Interface (sshell.c)
 * 
 * A basic shell implementation that accepts user commands and executes
 * them in separate child processes using fork() and execvp().
 * 
 * Features:
 * - Command parsing and execution
 * - Background process execution with '&'
 * - Proper error handling
 * - Exit command to terminate shell
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAX_LINE 80   // maximum command line length
#define MAX_ARGS 40   // maximum number of arguments

int main(void) {
    char input[MAX_LINE];                // buffer for user input
    char *args[MAX_ARGS + 1];            // command + arguments array
    int should_run = 1;                  // loop control variable

    printf("Simple Shell Interface\n");
    printf("Type 'exit' to quit, add '&' for background execution\n\n");

    while (should_run) {
        printf("osh> ");
        fflush(stdout);

        // Read user input
        if (!fgets(input, MAX_LINE, stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break;  // Handle Ctrl+D (EOF)
            }
            perror("Error reading input");
            continue;
        }

        // Remove trailing newline character
        input[strcspn(input, "\n")] = '\0';

        // Check for empty input
        if (strlen(input) == 0) {
            continue;
        }

        // Check for "exit" command
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            break;
        }

        // Parse input into tokens
        int arg_count = 0;
        char *token = strtok(input, " \t");  // Split on spaces and tabs
        int background = 0;
        
        while (token != NULL && arg_count < MAX_ARGS) {
            if (strcmp(token, "&") == 0) {
                background = 1;  // Mark for background execution
            } else {
                args[arg_count++] = token;
            }
            token = strtok(NULL, " \t");
        }
        args[arg_count] = NULL;  // Null-terminate the arguments array

        // Skip if no valid arguments
        if (arg_count == 0) {
            continue;
        }

        // Fork a new process
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            continue;
        } else if (pid == 0) {
            // Child process: execute the command
            if (execvp(args[0], args) == -1) {
                perror("Error executing command");
                exit(1);
            }
        } else {
            // Parent process: wait for child if not background
            if (!background) {
                int status;
                waitpid(pid, &status, 0);
            } else {
                printf("Background process started with PID: %d\n", pid);
            }
        }
    }

    printf("Shell terminated.\n");
    return 0;
}
