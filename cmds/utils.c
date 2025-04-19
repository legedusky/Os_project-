#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <bluetooth/bluetooth.h>

// ANSI color codes
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_RESET   "\033[0m"

// Clear the terminal screen
void clear_screen() {
    printf("\033[2J\033[H");
}

// Print a centered header
void print_header(const char *title) {
    int width = 80;  // Default terminal width
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        width = w.ws_col;
    }

    int padding = (width - strlen(title) - 2) / 2;
    printf("\n%s", COLOR_CYAN);
    for (int i = 0; i < width; i++) printf("═");
    printf("\n║%*s%s%*s║\n", padding, "", title, padding, "");
    for (int i = 0; i < width; i++) printf("═");
    printf("%s\n", COLOR_RESET);
}

// Print centered text
void print_centered(const char *text) {
    int width = 80;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        width = w.ws_col;
    }

    int padding = (width - strlen(text)) / 2;
    printf("%*s%s\n", padding, "", text);
}

// Print colored error message
void print_error(const char *message) {
    printf("%sERROR: %s%s\n", COLOR_RED, message, COLOR_RESET);
}

// Print colored warning
void print_warning(const char *message) {
    printf("%sWARNING: %s%s\n", COLOR_YELLOW, message, COLOR_RESET);
}

// Print success message
void print_success(const char *message) {
    printf("%s✓ %s%s\n", COLOR_GREEN, message, COLOR_RESET);
}

// Print info message
void print_info(const char *message) {
    printf("%sℹ %s%s\n", COLOR_BLUE, message, COLOR_RESET);
}

// Print device table
void print_device_table(bluetooth_device *devices, int count) {
    printf("\n%sDEVICE LIST:%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%-4s %-18s %-8s %-35s %s\n", "NUM", "ADDRESS", "STATUS", "NAME", "SIGNAL");
    printf("-------------------------------------------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        printf("%-4d %-18s [%s%-6s%s] %-35s ", 
               i + 1,
               devices[i].addr,
               devices[i].is_paired ? COLOR_GREEN : COLOR_YELLOW,
               devices[i].is_paired ? "PAIRED" : "NEW",
               COLOR_RESET,
               devices[i].name);
        
        // Show signal strength if available
        if (devices[i].rssi != 0) {
            printf("%s%d dBm%s", COLOR_MAGENTA, devices[i].rssi, COLOR_RESET);
        }
        printf("\n");
    }
}

// Print connection menu
void print_connection_menu() {
    printf("\n%sCONNECTION OPTIONS:%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  [1-%d] - Connect to device\n", MAX_DEVICES);
    printf("  [r]    - Rescan for devices\n");
    printf("  [s]    - Show system Bluetooth status\n");
    printf("  [q]    - Quit\n");
}