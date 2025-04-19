#include <stdio.h>
#include <stdlib.h>

int main() {
    // Execute the "df -h" command to show disk space in human-readable format
    int result = system("df -h");

    // Check if the command ran successfully
    if (result != 0) {
        perror("Error executing disk space command");
        return 1;
    }

    return 0;
}
