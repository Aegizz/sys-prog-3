#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "smsh.h"
#include <fcntl.h>
#include <dirent.h>
#include <fnmatch.h>
#include <glob.h>

// Define default prompt and constants for maximum commands and command length
#define DFL_PROMPT "> "
#define MAX_CMDS 1000
#define MAX_CMD_LEN 1024

// Function to duplicate a string
char *strdup(const char *src) {
    char *dst = malloc(strlen(src) + 1);  // Allocate space for the new string
    if (dst == NULL) return NULL;         // Check if memory allocation failed
    strcpy(dst, src);                     // Copy the string to the new location
    return dst;                           // Return the duplicated string
}

// Function to handle globbing for wildcard characters in arguments
char **handle_globbing(char **arglist) {
    char **newArglist = malloc(MAX_CMD_LEN * sizeof(char *));  // Allocate memory for the new argument list
    if (newArglist == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    int newArgIndex = 0;

    for (int argIndex = 0; arglist[argIndex] != NULL; argIndex++) {
        // Check for wildcard characters
        if (strchr(arglist[argIndex], '*') != NULL || strchr(arglist[argIndex], '?') != NULL) {
            glob_t globbuf;
            int glob_flags = 0;

            // Perform globbing
            if (glob(arglist[argIndex], glob_flags, NULL, &globbuf) == 0) {
                // Add matched paths to the new argument list
                for (int i = 0; i < globbuf.gl_pathc; i++) {
                    newArglist[newArgIndex] = strdup(globbuf.gl_pathv[i]);
                    if (newArglist[newArgIndex] == NULL) {
                        perror("strdup");
                        exit(EXIT_FAILURE);
                    }
                    newArgIndex++;
                }
            } else {
                perror("glob");
            }
            globfree(&globbuf);
        } else {
            // Add the argument to the new argument list
            newArglist[newArgIndex] = strdup(arglist[argIndex]);
            if (newArglist[newArgIndex] == NULL) {
                perror("strdup");
                exit(EXIT_FAILURE);
            }
            newArgIndex++;
        }
    }
    newArglist[newArgIndex] = NULL;  // Null-terminate the new argument list
    return newArglist;
}

// Function to check for redirection operators and handle them
char **check_redirect(char **arglist) {
    int i = 0;
    while (arglist[i] != NULL) {
        // Check for output redirection
        if (strcmp(arglist[i], ">") == 0) {
            int fd = open(arglist[i + 1], O_CREAT | O_WRONLY, 0777);  // Create or open the file
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {  // Redirect stdout to the file
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }
            arglist[i] = NULL;
            arglist[i + 1] = NULL;
        // Check for input redirection
        } else if (strcmp(arglist[i], "<") == 0) {
            int fd = open(arglist[i + 1], O_RDONLY);  // Open the file for reading
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDIN_FILENO) == -1) {  // Redirect stdin to the file
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);
            arglist[i] = NULL;
            arglist[i + 1] = NULL;
        }
        i++;
    }
    return arglist;
}

// Function to execute a pipeline of commands
int execute_pipeline(char *cmds[], int num_cmds) {
    int pipes[num_cmds - 1][2];  // Array to hold pipe file descriptors
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
            if (i != 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {  // Redirect stdin from the previous pipe
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (i != num_cmds - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {  // Redirect stdout to the next pipe
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Close unused pipe ends in child processes
            for (int j = 0; j < num_cmds - 1; j++) {
                if ((i != j) && (i != j + 1)) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }

            // Execute the command
            char *args[MAX_CMD_LEN];
            char *token = strtok(cmds[i], " ");
            int arg_idx = 0;
            while (token != NULL) {
                args[arg_idx++] = token;
                token = strtok(NULL, " ");
            }
            char **newArgs = handle_globbing(args);
            execvp(newArgs[0], newArgs);  // Replace the process image with the command
            perror("execvp");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipe ends in parent process
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

// Main function
int main() {
    char *cmdline, *prompt, **arglist;
    int result;
    void setup();

    prompt = DFL_PROMPT;  // Set the prompt
    setup();  // Initialize the shell

    // Main loop to read and execute commands
    while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
        // Split the command line based on "|"
        if ((arglist = splitline2(cmdline, "|")) != NULL) {
            arglist = check_redirect(arglist);  // Check for redirection
            if (arglist != NULL) {
                // Count the number of commands
                int num_cmds = 0;
                while (arglist[num_cmds] != NULL) {
                    num_cmds++;
                }
                // Execute the pipeline or single command
                result = execute_pipeline(arglist, num_cmds);
            }
            // Free the memory allocated for cmdline and arglist
            free(cmdline);
            freelist(arglist);
        }
    }
    return 0;
}

// Function to set up signal handling
void setup() {
    signal(SIGINT, SIG_IGN);  // Ignore SIGINT (Ctrl+C)
    signal(SIGQUIT, SIG_IGN);  // Ignore SIGQUIT (Ctrl+\)
}

// Function to handle fatal errors
void fatal(char *s1, char *s2, int n) {
    fprintf(stderr, "Error: %s, %s\n", s1, s2);  // Print error message
    exit(n);  // Exit with the specified status
}
