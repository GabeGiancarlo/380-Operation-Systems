# Simple Shell Interface (sshell.c)

A basic shell implementation that accepts user commands and executes them in separate child processes using `fork()` and `execvp()`.

## Features

- Command parsing and execution
- Background process execution with `&`
- Proper error handling for fork and exec operations
- Exit command to terminate the shell
- Support for standard Linux/UNIX commands

## How to Build

```bash
gcc sshell.c -o sshell
```

## How to Run

```bash
./sshell
```

## Usage

- Type any Linux/UNIX command at the "osh>" prompt
- Add `&` at the end of a command to run it in the background
- Type `exit` to quit the shell
- Press Ctrl+D to exit (EOF handling)

## Examples

```bash
osh> ls -l                    # List files in long format
osh> cat file.txt             # Display file contents
osh> ps -ael                  # Show all processes
osh> sleep 5 &                # Run sleep command in background
osh> exit                     # Quit the shell
```

## Implementation Details

- Uses `fork()` to create child processes
- Uses `execvp()` to execute commands
- Implements proper parent-child process synchronization
- Handles background execution with `&` operator
- Includes comprehensive error handling

## Error Handling

The program handles various error conditions:
- Fork failures
- Command execution errors
- Input/output errors
- Empty or invalid commands
