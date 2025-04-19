// sendfile.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include "sendfile.h"

#define PORT 8080
#define BUFFER_SIZE 8192

// Function to get file size
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}

// Serve the file using raw HTTP response
void sendfile_server(const char *filename) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);

    char buffer[BUFFER_SIZE];

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        return;
    }

    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return;
    }

    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        return;
    }

    // Print the link
    char ip[INET_ADDRSTRLEN] = "localhost";
    FILE *fp = popen("hostname -I | awk '{print $1}'", "r");
    if (fp) {
        fgets(ip, sizeof(ip), fp);
        strtok(ip, "\n"); // Remove newline
        pclose(fp);
    }

    printf("📂 File server running!\n📡 Share this link: http://%s:%d/%s\n", ip, PORT, filename);

    // Accept incoming connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (client_fd < 0) {
        perror("Accept failed");
        close(server_fd);
        return;
    }

    // Read request (ignore contents)
    read(client_fd, buffer, BUFFER_SIZE);

    // Open the file
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("File open failed");
        close(client_fd);
        close(server_fd);
        return;
    }

    long file_size = get_file_size(filename);

    // Send HTTP headers
    sprintf(buffer,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: %ld\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "\r\n", file_size, filename);
    write(client_fd, buffer, strlen(buffer));

    // Send file data
    int bytes;
    while ((bytes = read(fd, buffer, BUFFER_SIZE)) > 0)
        write(client_fd, buffer, bytes);

    printf("✅ File sent successfully!\n");

    close(fd);
    close(client_fd);
    close(server_fd);
}
