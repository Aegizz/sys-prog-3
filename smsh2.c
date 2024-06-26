/**  smsh1.c  small-shell version 1
 **  first really useful version after prompting shell
 **  this one parses the command line into strings
 **  uses fork, exec, wait, and ignores signals
 **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "smsh.h"

#define DFL_PROMPT "> "  // Default shell prompt
#define MAX_CMDS 1000    // Maximum number of commands in a pipeline
#define MAX_CMD_LEN 1024 // Maximum length of a single command

/**
 * Executes a pipeline of commands.
 *
 * @param cmds An array of command strings.
 * @param num_cmds The number of commands in the pipeline.
 */
void execute_pipeline(char *cmds[], int num_cmds) {
    int pipes[num_cmds - 1][2]; // Array to hold pipe file descriptors

    // Create pipes for communication between commands
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Loop to fork processes and set up pipes
    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (i != 0) {
                // Redirect stdin to the read end of the previous pipe
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (i != num_cmds - 1) {
                // Redirect stdout to the write end of the current pipe
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            // Close all pipe ends in the child process
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Parse the command into arguments
            char *args[MAX_CMD_LEN];
            char *token = strtok(cmds[i], " ");
            int arg_idx = 0;
            while (token != NULL) {
                args[arg_idx++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_idx] = NULL;

            // Execute the command
            execvp(args[0], args);
            perror("execvp");
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
}

int main()
{
    char *cmdline, *prompt, **arglist;
    int result;
    void setup();

    prompt = DFL_PROMPT;
    setup();

    while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
        // Split the command line based on "|"
        arglist = splitline2(cmdline, "|");
        if (arglist != NULL) {
            // Count the number of commands
            int num_cmds = 0;
            while (arglist[num_cmds] != NULL) {
                num_cmds++;
            }

            // Execute the pipeline or single command
            if (num_cmds > 1) {
                execute_pipeline(arglist, num_cmds);
            } else {
                result = execute(arglist);
            }

            // Free the memory allocated for arglist
            freelist(arglist);
        }
        // Free the memory allocated for cmdline
        free(cmdline);
    }
    return 0;
}

/**
 * Sets up the shell by ignoring interrupt signals.
 */
void setup()
{
    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

/**
 * Prints an error message and exits the program.
 *
 * @param s1 The first part of the error message.
 * @param s2 The second part of the error message.
 * @param n The exit status.
 */
void fatal(char *s1, char *s2, int n)
{
    fprintf(stderr, "Error: %s,%s\n", s1, s2);
    exit(n);
}
