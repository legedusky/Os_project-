#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Command to open Chrome
    char *chrome_command = "google-chrome";

    // Check if arguments are passed (like opening a specific URL)
    if (argc > 1) {
        // If a URL is passed, append it to the command
        char *url = argv[1];
        execlp(chrome_command, chrome_command, url, (char *)NULL);
    } else {
        // If no arguments, just launch Chrome
        execlp(chrome_command, chrome_command, (char *)NULL);
    }

    // If execlp fails, print an error
    perror("chrome");
    return 1;
}
