#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void beep_alarm() {
    // Try multiple sound methods, preferring more reliable options first
    // Don't redirect stderr so we can see errors in debug mode
    int ret1 = system("paplay /usr/share/sounds/freedesktop/stereo/complete.oga");
    if (ret1 != 0) {
        int ret2 = system("paplay /usr/share/sounds/freedesktop/stereo/message.oga");
        if (ret2 != 0) {
            int ret3 = system("beep");
            if (ret3 != 0) {
                printf("\a");  // ASCII bell as final fallback
            }
        }
    }
    fflush(stdout);
    usleep(300000);  // Wait 300ms - longer pause for better sound distinction
    
    // Second beep
    system("paplay /usr/share/sounds/freedesktop/stereo/complete.oga || "
           "paplay /usr/share/sounds/freedesktop/stereo/message.oga || "
           "beep || "
           "printf '\a'");
}

void show_reminder(const char *message) {
    // Write to all possible TTYs to ensure visibility
    const char *tty_paths[] = {"/dev/tty", "/dev/pts/0", "/dev/pts/1", "/dev/pts/2", NULL};
    int shown = 0;
    
    for (int i = 0; tty_paths[i] != NULL; i++) {
        FILE *tty = fopen(tty_paths[i], "w");
        if (tty) {
            fprintf(tty, "\n\033[1;33m"); // Bright yellow
            fprintf(tty, "🔔 ================== REMINDER ================== 🔔\n");
            fprintf(tty, "📝 %s\n", message);
            fprintf(tty, "🔔 ============================================= 🔔\n");
            fprintf(tty, "\033[0m\n"); // Reset color
            fflush(tty);
            fclose(tty);
            shown = 1;
        }
    }

    // If no TTY worked, try using notify-send
    if (!shown) {
        char cmd[1024];
        // Use single quotes around the message to preserve spaces and special characters
        snprintf(cmd, sizeof(cmd), "notify-send -u critical \"Reminder\" '%s'", message);
        system(cmd);
    }

    beep_alarm();
}

int parse_time_to_seconds(const char *time_str) {
    int hour, min;
    char meridian[3];
    if (sscanf(time_str, "%d:%d%2s", &hour, &min, meridian) != 3) {
        fprintf(stderr, "remind: Invalid time format. Use HH:MMam or HH:MMpm\n");
        return -1;
    }

    if (strcasecmp(meridian, "pm") == 0 && hour != 12) hour += 12;
    if (strcasecmp(meridian, "am") == 0 && hour == 12) hour = 0;

    time_t now = time(NULL);
    struct tm *current = localtime(&now);
    struct tm target = *current;
    target.tm_hour = hour;
    target.tm_min = min;
    target.tm_sec = 0;

    time_t target_time = mktime(&target);
    if (target_time == -1) return -1;

    int seconds = (int)difftime(target_time, now);
    if (seconds < 0) {
        fprintf(stderr, "remind: That time has already passed today!\n");
        return -1;
    }

    return seconds;
}

int parse_datetime_to_seconds(const char *date_str, const char *time_str) {
    int year, month, day, hour, min;
    if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3 ||
        sscanf(time_str, "%d:%d", &hour, &min) != 2) {
        fprintf(stderr, "remind: Invalid date/time format. Use YYYY-MM-DD HH:MM\n");
        return -1;
    }

    struct tm target = {0};
    target.tm_year = year - 1900;
    target.tm_mon = month - 1;
    target.tm_mday = day;
    target.tm_hour = hour;
    target.tm_min = min;
    target.tm_sec = 0;

    time_t now = time(NULL);
    time_t target_time = mktime(&target);
    if (target_time == -1) return -1;

    int seconds = (int)difftime(target_time, now);
    if (seconds < 0) {
        fprintf(stderr, "remind: That date/time has already passed!\n");
        return -1;
    }

    return seconds;
}

int daemonize() {
    // First fork
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) return 1;  // Parent exits
    
    // Create new session
    if (setsid() < 0) return -1;
    
    // Second fork
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(EXIT_SUCCESS);
    
    // Change working directory
    chdir("/");
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect remaining I/O to /dev/null
    open("/dev/null", O_RDWR); // stdin
    dup(0); // stdout
    dup(0); // stderr
    
    return 0;
}

// Function to join arguments into a single string
char* join_strings(char** argv, int start, int end) {
    // Calculate total length needed
    size_t total_len = 0;
    for (int i = start; i < end; i++) {
        total_len += strlen(argv[i]) + 1; // +1 for space or null terminator
    }
    
    char* result = malloc(total_len);
    if (!result) return NULL;
    
    // Concatenate all strings
    result[0] = '\0'; // Initialize with empty string
    for (int i = start; i < end; i++) {
        if (i > start) strcat(result, " "); // Add space between words
        strcat(result, argv[i]);
    }
    
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  remind <minutes> \"Your message\"\n");
        fprintf(stderr, "  remind at HH:MMam/pm \"Your message\"\n");
        fprintf(stderr, "  remind at YYYY-MM-DD HH:MM \"Your message\"\n");
        return 1;
    }

    int delay_seconds = 0;
    char *message = NULL;
    int msg_start_idx = 0;

    if (strcmp(argv[1], "at") == 0) {
        if (argc < 4) {
            fprintf(stderr, "remind: Invalid format for 'at'.\n");
            return 1;
        }
        
        if (strchr(argv[2], ':') && (strstr(argv[2], "am") || strstr(argv[2], "pm"))) {
            // Time format: HH:MMam/pm
            delay_seconds = parse_time_to_seconds(argv[2]);
            msg_start_idx = 3;
        }
        else if (argc >= 5 && strchr(argv[2], '-') && strchr(argv[3], ':')) {
            // Date+time format: YYYY-MM-DD HH:MM
            delay_seconds = parse_datetime_to_seconds(argv[2], argv[3]);
            msg_start_idx = 4;
        }
        else {
            fprintf(stderr, "remind: Invalid format for 'at'.\n");
            return 1;
        }

        if (delay_seconds < 0) return 1;
    } else {
        int minutes = atoi(argv[1]);
        if (minutes <= 0) {
            fprintf(stderr, "remind: Invalid minutes value.\n");
            return 1;
        }
        delay_seconds = minutes * 60;
        msg_start_idx = 2;
    }

    // Make sure we have a message
    if (msg_start_idx >= argc) {
        fprintf(stderr, "remind: Missing message.\n");
        return 1;
    }

    // Join all remaining arguments as the message
    message = join_strings(argv, msg_start_idx, argc);
    if (!message) {
        fprintf(stderr, "remind: Memory allocation failed.\n");
        return 1;
    }

    // Daemonize the process
    int daemon_result = daemonize();
    if (daemon_result == 1) {  // Parent process
        printf("Reminder set (PID: %d) for message: %s\n", getpid(), message);
        free(message);  // Free memory in parent process
        return 0;
    }
    if (daemon_result == -1) {
        perror("remind: daemonization failed");
        free(message);
        return 1;
    }

    // Child process continues as daemon
    sleep(delay_seconds);
    show_reminder(message);
    free(message);  // Free memory in child process
    
    return 0;
}