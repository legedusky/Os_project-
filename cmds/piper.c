#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX 1024

void execute_pipe(char *cmd1[], char *cmd2[]) {
    int pipefd[2];
    pid_t p1, p2;

    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(1);
    }

    p1 = fork();
    if (p1 == 0) {
        // First child -> executes cmd1 and writes to pipe
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[1]);
        execvp(cmd1[0], cmd1);
        perror("execvp");
        exit(1);
    }

    p2 = fork();
    if (p2 == 0) {
        // Second child -> reads from pipe and executes cmd2
        close(pipefd[1]); // Close write end
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin from pipe
        close(pipefd[0]);
        execvp(cmd2[0], cmd2);
        perror("execvp");
        exit(1);
    }

    // Parent process closes pipe and waits
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}

char **parse_args(char *input) {
    char **args = malloc(sizeof(char *) * MAX);
    char *token = strtok(input, " \t\n");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return args;
}

int main() {
    char input[MAX];

    printf(">> ");
    fgets(input, MAX, stdin);

    // Split by pipe
    char *first = strtok(input, "|");
    char *second = strtok(NULL, "|");

    if (first && second) {
        char **cmd1 = parse_args(first);
        char **cmd2 = parse_args(second);
        execute_pipe(cmd1, cmd2);
        free(cmd1);
        free(cmd2);
    } else {
        printf("Pipe not found or invalid command\n");
    }

    return 0;
}
