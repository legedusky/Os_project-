#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: portcheck [port]\n");
        return 1;
    }

    // The port to check
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Please provide a valid port number between 1 and 65535.\n");
        return 1;
    }

    // Command to check if the port is open
    char command[100];
    snprintf(command, sizeof(command), "ss -ltn | grep :%d", port);  // Using ss for more modern systems

    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Error opening pipe");
        return 1;
    }

    // Read the output of the command
    char output[256];
    if (fgets(output, sizeof(output), fp) != NULL) {
        // Port is open, show the process info
        printf("Port %d is open. Checking the process...\n", port);
        fclose(fp);

        // Now, use lsof or ps to find which process is using the port
        snprintf(command, sizeof(command), "lsof -i :%d", port);
        fp = popen(command, "r");
        if (fp == NULL) {
            perror("Error opening pipe");
            return 1;
        }

        printf("Process using port %d:\n", port);
        while (fgets(output, sizeof(output), fp) != NULL) {
            printf("%s", output);
        }

        fclose(fp);
    } else {
        // Port is not open
        printf("Port %d is not open or no process is using it.\n", port);
        fclose(fp);
    }

    return 0;
}
