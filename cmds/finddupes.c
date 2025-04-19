#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#define MAX_PATH 4096
#define BUFFER_SIZE 8192
#define MAX_FILES_PER_GROUP 100

// Structure to store file information
typedef struct FileInfo {
    char path[MAX_PATH];
    off_t size;
    unsigned long hash;
    struct FileInfo* next;
} FileInfo;

// Structure to store duplicate groups
typedef struct DuplicateGroup {
    FileInfo* files[MAX_FILES_PER_GROUP];
    int count;
    struct DuplicateGroup* next;
} DuplicateGroup;

// Hash table to store files by size
typedef struct {
    FileInfo* files[1024];
    DuplicateGroup* duplicates;
} HashTable;

// Get absolute path
char* get_absolute_path(const char* path, char* resolved_path) {
    if (path[0] == '/') {
        // Path is already absolute
        strncpy(resolved_path, path, PATH_MAX - 1);
        resolved_path[PATH_MAX - 1] = '\0';
    } else {
        // Get current working directory and append relative path
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("minsh: getcwd");
            return NULL;
        }
        
        size_t cwd_len = strlen(cwd);
        size_t path_len = strlen(path);
        
        if (cwd_len + path_len + 2 > PATH_MAX) {
            fprintf(stderr, "minsh: Path too long\n");
            return NULL;
        }
        
        strcpy(resolved_path, cwd);
        resolved_path[cwd_len] = '/';
        strcpy(resolved_path + cwd_len + 1, path);
    }
    
    // Resolve any '.', '..', or symbolic links
    char* result = realpath(resolved_path, NULL);
    if (result) {
        strncpy(resolved_path, result, PATH_MAX - 1);
        resolved_path[PATH_MAX - 1] = '\0';
        free(result);
        return resolved_path;
    }
    
    return NULL;
}

// Calculate enhanced checksum of a file
unsigned long calculate_checksum(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    unsigned char buffer[BUFFER_SIZE];
    unsigned long checksum = 0;
    size_t bytes_read;
    unsigned long position = 0;

    // Read file in larger chunks for better performance
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            // Enhanced checksum calculation using both position and byte value
            checksum = ((checksum << 4) + buffer[i]) % 0xFFFFFFFF;
            unsigned long work = (checksum & 0xF0000000) >> 24;
            if (work) {
                checksum ^= work;
            }
            checksum = (checksum + position++) % 0xFFFFFFFF;
        }
    }

    fclose(file);
    return checksum;
}

// Add duplicate group to the list
void add_duplicate_group(HashTable* table, FileInfo* file1, FileInfo* file2) {
    DuplicateGroup* group = malloc(sizeof(DuplicateGroup));
    group->count = 2;
    group->files[0] = file1;
    group->files[1] = file2;
    group->next = table->duplicates;
    table->duplicates = group;
}

// Add file to existing duplicate group or create new group
void handle_duplicate(HashTable* table, FileInfo* file1, FileInfo* file2) {
    // Check if either file is already in a duplicate group
    DuplicateGroup* current = table->duplicates;
    while (current != NULL) {
        for (int i = 0; i < current->count; i++) {
            if (strcmp(current->files[i]->path, file1->path) == 0 ||
                strcmp(current->files[i]->path, file2->path) == 0) {
                // Add the other file to this group if not already present
                int found = 0;
                for (int j = 0; j < current->count; j++) {
                    if (strcmp(current->files[j]->path, file2->path) == 0 ||
                        strcmp(current->files[j]->path, file1->path) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found && current->count < MAX_FILES_PER_GROUP) {
                    current->files[current->count++] = 
                        (strcmp(current->files[i]->path, file1->path) == 0) ? file2 : file1;
                }
                return;
            }
        }
        current = current->next;
    }
    
    // If no existing group found, create new group
    add_duplicate_group(table, file1, file2);
}

// Add file to hash table
void add_file(HashTable* table, const char* path, off_t size) {
    char abs_path[PATH_MAX];
    if (!get_absolute_path(path, abs_path)) {
        fprintf(stderr, "minsh: Cannot resolve path for %s: %s\n", path, strerror(errno));
        return;
    }
    
    int index = size % 1024;
    
    // Create new file info
    FileInfo* info = malloc(sizeof(FileInfo));
    strncpy(info->path, abs_path, MAX_PATH - 1);
    info->path[MAX_PATH - 1] = '\0';
    info->size = size;
    info->hash = 0;
    info->next = NULL;

    // Calculate hash only if there's already a file with the same size
    FileInfo* current = table->files[index];
    while (current != NULL) {
        if (current->size == size) {
            // Calculate hash for new file
            if (info->hash == 0) {
                info->hash = calculate_checksum(info->path);
                if (info->hash == 0) {
                    free(info);
                    return;
                }
            }
            // Calculate hash for existing file if not already calculated
            if (current->hash == 0) {
                current->hash = calculate_checksum(current->path);
                if (current->hash == 0) {
                    continue;
                }
            }
            // Compare hashes and do byte-by-byte comparison for extra verification
            if (info->hash == current->hash) {
                // Do a byte-by-byte comparison to ensure they are truly identical
                FILE *f1 = fopen(info->path, "rb");
                FILE *f2 = fopen(current->path, "rb");
                if (f1 && f2) {
                    int match = 1;
                    int c1, c2;
                    while ((c1 = fgetc(f1)) != EOF && (c2 = fgetc(f2)) != EOF) {
                        if (c1 != c2) {
                            match = 0;
                            break;
                        }
                    }
                    fclose(f1);
                    fclose(f2);
                    if (match) {
                        handle_duplicate(table, current, info);
                    }
                }
            }
        }
        current = current->next;
    }

    // Add new file to hash table
    info->next = table->files[index];
    table->files[index] = info;
}

// Scan directory recursively
void scan_directory(const char* dir_path, HashTable* table) {
    char abs_path[PATH_MAX];
    if (!get_absolute_path(dir_path, abs_path)) {
        fprintf(stderr, "minsh: Cannot resolve path for %s: %s\n", dir_path, strerror(errno));
        return;
    }

    DIR* dir = opendir(abs_path);
    if (!dir) {
        fprintf(stderr, "minsh: Cannot open directory %s: %s\n", abs_path, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full path safely
        char path[PATH_MAX];
        size_t abs_len = strlen(abs_path);
        size_t name_len = strlen(entry->d_name);
        
        if (abs_len + name_len + 2 > PATH_MAX) {
            fprintf(stderr, "minsh: Path too long: %s/%s\n", abs_path, entry->d_name);
            continue;
        }
        
        strcpy(path, abs_path);
        path[abs_len] = '/';
        strcpy(path + abs_len + 1, entry->d_name);

        struct stat st;
        if (lstat(path, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively scan subdirectories
            scan_directory(path, table);
        } else if (S_ISREG(st.st_mode)) {
            // Add regular files to hash table
            add_file(table, path, st.st_size);
        }
    }

    closedir(dir);
}

// Handle user interaction for duplicate files
void handle_duplicates(HashTable* table) {
    DuplicateGroup* current = table->duplicates;
    int group_num = 1;
    
    while (current != NULL) {
        printf("\nDuplicate group %d:\n", group_num++);
        for (int i = 0; i < current->count; i++) {
            printf("[%d] %s (%.2f MB)\n", i + 1, current->files[i]->path, 
                   (double)current->files[i]->size / (1024 * 1024));
        }
        
        printf("\nWould you like to delete any of these duplicates? (y/n): ");
        char response;
        scanf(" %c", &response);
        response = tolower(response);
        
        if (response == 'y') {
            printf("Enter the numbers of files to delete (space-separated, or 0 to skip): ");
            char line[256];
            getchar(); // Clear newline
            if (fgets(line, sizeof(line), stdin)) {
                char* token = strtok(line, " \n");
                while (token != NULL) {
                    int num = atoi(token);
                    if (num > 0 && num <= current->count) {
                        printf("Deleting: %s\n", current->files[num-1]->path);
                        if (unlink(current->files[num-1]->path) != 0) {
                            fprintf(stderr, "Error deleting %s: %s\n", 
                                    current->files[num-1]->path, strerror(errno));
                        }
                    }
                    token = strtok(NULL, " \n");
                }
            }
        }
        
        current = current->next;
    }
}

// Free hash table memory
void cleanup_hash_table(HashTable* table) {
    // Free file info structures
    for (int i = 0; i < 1024; i++) {
        FileInfo* current = table->files[i];
        while (current != NULL) {
            FileInfo* next = current->next;
            free(current);
            current = next;
        }
    }
    
    // Free duplicate groups
    DuplicateGroup* current = table->duplicates;
    while (current != NULL) {
        DuplicateGroup* next = current->next;
        free(current);
        current = next;
    }
}

int main(int argc, char** argv) {
    // Initialize hash table
    HashTable table = {0};
    table.duplicates = NULL;

    // Get directory to scan
    const char* scan_dir = argc > 1 ? argv[1] : ".";
    
    char abs_path[PATH_MAX];
    if (!get_absolute_path(scan_dir, abs_path)) {
        fprintf(stderr, "minsh: Cannot resolve path for %s: %s\n", scan_dir, strerror(errno));
        return 1;
    }

    printf("Scanning for duplicates in: %s\n", abs_path);
    
    // Scan directory
    scan_directory(scan_dir, &table);
    
    // Handle duplicates interactively
    if (table.duplicates != NULL) {
        handle_duplicates(&table);
    } else {
        printf("No duplicates found.\n");
    }

    // Cleanup
    cleanup_hash_table(&table);

    return 0;
} 