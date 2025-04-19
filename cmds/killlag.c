#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_PROCESSES 1024
#define MAX_LINE 256
#define TOP_N 5  // Show only top 5 processes

// Structure to store process information
typedef struct {
    pid_t pid;
    char command[MAX_LINE];
    char user[32];
    unsigned long memory;  // in KB
    float cpu;
} ProcessInfo;

// Get username from UID
void get_username(uid_t uid, char* username) {
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        strncpy(username, pw->pw_name, 31);
        username[31] = '\0';
    } else {
        snprintf(username, 32, "%d", uid);
    }
}

// Read process command from /proc/[pid]/cmdline
void get_process_command(pid_t pid, char* command) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    FILE* f = fopen(path, "r");
    if (f) {
        size_t n = fread(command, 1, MAX_LINE - 1, f);
        if (n > 0) {
            command[n] = '\0';
            // Replace null bytes with spaces
            for (size_t i = 0; i < n; i++) {
                if (command[i] == '\0') command[i] = ' ';
            }
        } else {
            // If cmdline is empty, try comm
            fclose(f);
            snprintf(path, sizeof(path), "/proc/%d/comm", pid);
            f = fopen(path, "r");
            if (f) {
                if (fgets(command, MAX_LINE, f)) {
                    // Remove newline
                    command[strcspn(command, "\n")] = 0;
                } else {
                    strcpy(command, "[unknown]");
                }
            }
        }
        fclose(f);
    } else {
        strcpy(command, "[unknown]");
    }
}

// Get process memory usage from /proc/[pid]/status
unsigned long get_process_memory(pid_t pid) {
    char path[64];
    char line[256];
    unsigned long memory = 0;
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %lu", &memory);
                break;
            }
        }
        fclose(f);
    }
    return memory;
}

// Get process CPU usage from /proc/[pid]/stat
float get_process_cpu(pid_t pid) {
    char path[64];
    char line[512];
    unsigned long utime = 0, stime = 0;
    
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE* f = fopen(path, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            char* ptr = strrchr(line, ')');
            if (ptr) {
                sscanf(ptr + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                       &utime, &stime);
            }
        }
        fclose(f);
    }
    
    // Convert to percentage (rough estimate)
    return (utime + stime) / (float)sysconf(_SC_CLK_TCK);
}

// Get process owner
void get_process_owner(pid_t pid, char* user) {
    char path[64];
    struct stat st;
    
    snprintf(path, sizeof(path), "/proc/%d", pid);
    if (stat(path, &st) == 0) {
        get_username(st.st_uid, user);
    } else {
        strcpy(user, "?");
    }
}

// Compare function for qsort
int compare_cpu(const void* a, const void* b) {
    const ProcessInfo* p1 = (const ProcessInfo*)a;
    const ProcessInfo* p2 = (const ProcessInfo*)b;
    if (p2->cpu > p1->cpu) return 1;
    if (p2->cpu < p1->cpu) return -1;
    return 0;
}

// List processes sorted by CPU usage
int list_processes(ProcessInfo* processes) {
    DIR* proc = opendir("/proc");
    if (!proc) {
        perror("Cannot open /proc");
        return 0;
    }

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(proc)) && count < MAX_PROCESSES) {
        // Check if the directory entry is a process ID
        if (!isdigit(entry->d_name[0])) {
            continue;
        }

        pid_t pid = atoi(entry->d_name);
        
        // Skip current process
        if (pid == getpid()) {
            continue;
        }

        // Get process information
        get_process_command(pid, processes[count].command);
        get_process_owner(pid, processes[count].user);
        processes[count].memory = get_process_memory(pid);
        processes[count].cpu = get_process_cpu(pid);
        processes[count].pid = pid;
        
        count++;
    }

    closedir(proc);

    // Sort processes by CPU usage
    qsort(processes, count, sizeof(ProcessInfo), compare_cpu);

    return count;
}

void print_header() {
    printf("\033[1;36m"); // Cyan color for header
    printf("\n╔════════════════════ TOP %d CPU-CONSUMING PROCESSES ════════════════════╗\n", TOP_N);
    printf("║                                                                      ║\n");
    printf("║  %-6s %-15s %-10s %-12s %-8s %-15s ║\n", 
           "PID", "USER", "CPU%", "MEM(MB)", "NUMBER", "COMMAND");
    printf("║  %-71s ║\n", "──────────────────────────────────────────────────────────────");
    printf("\033[0m"); // Reset color
}

void print_footer() {
    printf("\033[1;36m"); // Cyan color for footer
    printf("║                                                                      ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    printf("\033[0m"); // Reset color
}

int main() {
    ProcessInfo processes[MAX_PROCESSES];
    char input[32];
    char response;
    int count;

    while (1) {
        // Clear screen
        printf("\033[2J\033[H");
        
        // List processes
        count = list_processes(processes);
        
        // Print header
        print_header();
        
        // Print top 5 processes
        int display_count = (count < TOP_N) ? count : TOP_N;
        for (int i = 0; i < display_count; i++) {
            printf("║  \033[1m%-6d %-15s \033[1;33m%-10.1f\033[0m %-12.1f [\033[1;32m%d\033[0m] %-15.15s ║\n",
                   processes[i].pid,
                   processes[i].user,
                   processes[i].cpu,
                   processes[i].memory / 1024.0,
                   i + 1,
                   processes[i].command);
        }
        
        // Print footer
        print_footer();
        
        printf("\nEnter process number to kill (1-%d) or 'q' to quit: ", display_count);
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Check for quit
        if (input[0] == 'q' || input[0] == 'Q') {
            break;
        }
        
        // Parse input
        int num = atoi(input);
        if (num < 1 || num > display_count) {
            printf("\033[1;31mInvalid process number! Please enter 1-%d\033[0m\n", display_count);
            printf("Press Enter to continue...");
            getchar();
            continue;
        }
        
        // Confirm kill
        printf("\n\033[1;31mWarning:\033[0m Kill process %d (\033[1m%s\033[0m)? (y/n): ", 
               processes[num-1].pid, processes[num-1].command);
        scanf(" %c", &response);
        getchar(); // Clear newline
        
        if (response == 'y' || response == 'Y') {
            if (kill(processes[num-1].pid, SIGTERM) == 0) {
                printf("\033[1;32mProcess %d terminated successfully\033[0m\n", processes[num-1].pid);
            } else {
                printf("\033[1;31m");
                perror("Failed to kill process");
                printf("\033[0m");
            }
            printf("Press Enter to continue...");
            getchar();
        }
    }
    
    return 0;
}
