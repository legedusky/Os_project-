#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#define MAX_FILES 10000

struct FileEntry {
    char path[PATH_MAX];
    time_t mtime;
};

struct FileEntry files[MAX_FILES];
int file_count = 0;

void collect_files(const char *base_path) {
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1)
            continue;

        if (S_ISDIR(st.st_mode)) {
            collect_files(full_path); // recurse
        } else if (S_ISREG(st.st_mode)) {
            if (file_count < MAX_FILES) {
                strncpy(files[file_count].path, full_path, PATH_MAX);
                files[file_count].mtime = st.st_mtime;
                file_count++;
            }
        }
    }

    closedir(dir);
}

int compare_files(const void *a, const void *b) {
    time_t t1 = ((struct FileEntry *)a)->mtime;
    time_t t2 = ((struct FileEntry *)b)->mtime;
    return (t2 - t1); // descending order
}

int main(int argc, char *argv[]) {
    int n = 10; // default number of files

    if (argc == 2) {
        n = atoi(argv[1]);
        if (n <= 0) {
            fprintf(stderr, "Usage: %s [n > 0]\n", argv[0]);
            return 1;
        }
    }

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    collect_files(cwd);

    if (file_count == 0) {
        printf("No files found.\n");
        return 0;
    }

    qsort(files, file_count, sizeof(struct FileEntry), compare_files);

    printf("Top %d most recently modified files:\n\n", n);
    for (int i = 0; i < n && i < file_count; i++) {
        char mod_time[64];
        struct tm *tm_info = localtime(&files[i].mtime);
        strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("%s\t%s\n", mod_time, files[i].path);
    }

    return 0;
}
