/**  smsh1.c  small-shell version 1
 **		first really useful version after prompting shell
 **		this one parses the command line into strings
 **		uses fork, exec, wait, and ignores signals
 **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "smsh.h"
#include <fcntl.h>

#define DFL_PROMPT "> "
#define MAX_CMDS 1000
#define MAX_CMD_LEN 1024

// Function to check for redirection operators (">" and "<") in the argument list
char **check_redirect(char **arglist) {
    int i = 0;
    while (arglist[i] != NULL) {
        // Check for output redirection ">"
        if (strcmp(arglist[i], ">") == 0) {
            // Open or create the file for writing
            int fd = open(arglist[i + 1], O_CREAT | O_WRONLY, 0777);
            if (fd == -1) {  // Check for errors
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redirect stdout to the file
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);  // Close the file descriptor
            arglist[i] = NULL;
            arglist[i + 1] = NULL;
        // Check for input redirection "<"
        } else if (strcmp(arglist[i], "<") == 0) {
            // Open the file for reading
            int fd = open(arglist[i + 1], O_RDONLY);
            if (fd == -1) {  // Check for errors
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redirect stdin to the file
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);  // Close the file descriptor
            arglist[i] = NULL;
            arglist[i + 1] = NULL;
        }
        i++;
    }
    return arglist;
}

// Function to execute a pipeline of commands
int execute_pipeline(char *cmds[], int num_cmds) {
    int pipes[num_cmds - 1][2];
    
    // Create pipes for inter-process communication
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();  // Fork a new process
        if (pid == 0) {
            // Child process

            // Redirect input from previous command's pipe
            if (i != 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            // Redirect output to next command's pipe
            if (i != num_cmds - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            // Close all pipe ends
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Prepare arguments for execvp
            char *args[MAX_CMD_LEN];
            char *token = strtok(cmds[i], " ");
            int arg_idx = 0;
            while (token != NULL) {
                args[arg_idx++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_idx] = NULL;

            // Check for redirection in the arguments
            char **newArgs = check_redirect(args);

            // Execute the command
            execvp(newArgs[0], newArgs);
            perror("execvp");  // If execvp returns, there was an error
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipe ends in the parent process
    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
    return 0;
}

int main() {
    char *cmdline, *prompt, **arglist;
    int result;
    void setup();

    prompt = DFL_PROMPT;
    setup();  // Initialize the shell

    while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
        // Split the command line based on "|"
        if ((arglist = splitline2(cmdline, "|")) != NULL) {
            if (arglist != NULL) {
                // Count the number of commands
                int num_cmds = 0;
                while (arglist[num_cmds] != NULL) {
                    num_cmds++;
                }

                // Execute the pipeline or single command
                result = execute_pipeline(arglist, num_cmds);

                // Free the memory allocated for arglist
                freelist(arglist);
            }
            // Free the memory allocated for cmdline
            free(cmdline);
        }
    }
    return 0;
}

void setup() {
    /*
     * Purpose: Initialize shell by ignoring interrupt and quit signals
     * Returns: Nothing. Calls fatal() if there is trouble
     */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

// Function to handle fatal errors
void fatal(char *s1, char *s2, int n) {
    fprintf(stderr, "Error: %s, %s\n", s1, s2);
    exit(n);
}
