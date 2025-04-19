#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: timeit <command> [args...]\n");
        return 1;
    }

    struct timeval start, end;

    // Get the start time
    gettimeofday(&start, NULL);

    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) {
        // In child process: execute the command
        execvp(argv[1], &argv[1]);
        perror("execvp"); // If execvp fails
        exit(1);
    } else if (pid > 0) {
        // In parent process: wait for child
        int status;
        waitpid(pid, &status, 0);

        // Get end time
        gettimeofday(&end, NULL);

        // Calculate time difference in milliseconds
        long seconds = end.tv_sec - start.tv_sec;
        long microseconds = end.tv_usec - start.tv_usec;
        double elapsed = seconds + microseconds / 1e6;

        printf("\nExecution time: %.6f seconds\n", elapsed);

        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    } else {
        perror("fork");
        return 1;
    }
}
