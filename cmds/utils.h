#ifndef UTILS_H
#define UTILS_H

#define MAX_DEVICES 255

typedef struct {
    char addr[19];
    char name[249];
    int is_paired;
    int rssi;
} bluetooth_device;

void clear_screen();
void print_header(const char *title);
void print_centered(const char *text);
void print_error(const char *message);
void print_warning(const char *message);
void print_success(const char *message);
void print_info(const char *message);
void print_device_table(bluetooth_device *devices, int count);
void print_connection_menu();

#endif