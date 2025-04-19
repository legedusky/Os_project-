#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EXTENSION_LEN 50

// Structure to store extension and count
typedef struct {
    char extension[MAX_EXTENSION_LEN];
    int count;
} ExtensionCount;

// Function to get the extension of a file
const char *get_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";  // No extension
    return dot + 1;  // Return part after the last dot
}

// Function to update the extension count
void update_extension_count(ExtensionCount *ext_counts, int *ext_count_len, const char *extension) {
    for (int i = 0; i < *ext_count_len; i++) {
        if (strcmp(ext_counts[i].extension, extension) == 0) {
            ext_counts[i].count++;
            return;
        }
    }

    // If the extension is not found, add it
    strncpy(ext_counts[*ext_count_len].extension, extension, MAX_EXTENSION_LEN);
    ext_counts[*ext_count_len].count = 1;
    (*ext_count_len)++;
}

// Function to list files in the directory recursively
void list_files(const char *path, ExtensionCount *ext_counts, int *ext_count_len) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip current (.) and parent (..) directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Recursively search in subdirectories
            list_files(full_path, ext_counts, ext_count_len);
        } else {
            // Update extension count for files
            const char *ext = get_extension(entry->d_name);
            if (strlen(ext) > 0) {
                update_extension_count(ext_counts, ext_count_len, ext);
            }
        }
    }

    closedir(dir);
}

int main() {
    // Array to store the extension counts
    ExtensionCount ext_counts[1000];
    int ext_count_len = 0;

    // Get the current working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    // List files and count extensions
    list_files(cwd, ext_counts, &ext_count_len);

    // Print the extension counts
    printf("Extension counts in directory: %s\n", cwd);
    for (int i = 0; i < ext_count_len; i++) {
        printf("%s: %d\n", ext_counts[i].extension, ext_counts[i].count);
    }

    return 0;
}
