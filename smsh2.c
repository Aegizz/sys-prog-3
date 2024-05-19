/**  smsh1.c  small-shell version 1
 **		first really useful version after prompting shell
 **		this one parses the command line into strings
 **		uses fork, exec, wait, and ignores signals
 **/

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include	"smsh.h"

#define	DFL_PROMPT	"> "
#define MAX_CMDS 1000
#define MAX_CMD_LEN 1024

int execute_pipeline(char *cmds[], int num_cmds) {
    int pipes[num_cmds - 1][2];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (i != 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (i != num_cmds - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Close all pipe ends in child processes except the ones it uses
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
            args[arg_idx] = NULL;

            execvp(args[0], args);
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

int main()
{
	char	*cmdline, *prompt, **arglist;
	int	result;
	void	setup();

	prompt = DFL_PROMPT ;
	setup();

    while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
        // Split the command line based on "|"
        arglist = splitline(cmdline);
        if (arglist != NULL) {
            // Count the number of commands
            int num_cmds = 0;
            while (arglist[num_cmds] != NULL) {
                num_cmds++;
            }

            // Execute the pipeline or single command
            if (num_cmds > 1) {
                result = execute_pipeline(arglist, num_cmds);
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

void setup()
/*
 * purpose: initialize shell
 * returns: nothing. calls fatal() if trouble
 */
{
	signal(SIGINT,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
}

void fatal(char *s1, char *s2, int n)
{
	fprintf(stderr,"Error: %s,%s\n", s1, s2);
	exit(n);
}

