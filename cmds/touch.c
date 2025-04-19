#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: touch <filename>\n");
        return 1;
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL, 0644);  // Create the file if it doesn't exist
    if (fd < 0) {
        perror("touch");
        return 1;
    }

    close(fd);
    return 0;
}
