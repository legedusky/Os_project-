#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>

#define MAX_DEVICES 255
#define SCAN_TIME 8  // 8 * 1.28 = 10.24 seconds
#define MAX_RETRIES 3

typedef struct {
    char addr[19];      // Bluetooth address (18 chars + null terminator)
    char name[249];     // Device name (248 chars + null terminator)
    int is_paired;
    int rssi;           // Signal strength
    uint8_t dev_class[3]; // Device class
} bluetooth_device;

// ANSI color codes
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_RESET   "\033[0m"

// Function prototypes
void check_bluetooth_hardware();
int ensure_bluetooth_enabled(int dev_id);
int get_paired_devices(int dev_id, bluetooth_device *devices, int *count);
int scan_devices(bluetooth_device *devices, int *count);
int connect_device(const char *addr);
void print_device_table(bluetooth_device *devices, int count);
void print_connection_menu();
void clear_screen();
void print_header(const char *title);
void print_footer();
void print_centered(const char *text);
void print_error(const char *message);
void print_success(const char *message);
void print_warning(const char *message);
void print_info(const char *message);

// Main function
int main() {
    bluetooth_device devices[MAX_DEVICES];
    int device_count = 0;
    char input[32];
    int retries = 0;
    
    clear_screen();
    print_header("BLUETOOTH DEVICE MANAGER");
    print_centered("Initializing Bluetooth...");
    
    while (1) {
        // Scan for devices
        device_count = 0;
        if (scan_devices(devices, &device_count) < 0) {
            print_error("Failed to scan for devices");
            if (retries++ >= MAX_RETRIES) {
                print_warning("Maximum retries reached. Exiting...");
                sleep(2);
                break;
            }
            sleep(1);
            continue;
        }
        retries = 0;

        clear_screen();
        print_header("AVAILABLE BLUETOOTH DEVICES");
        
        if (device_count <= 0) {
            print_centered("No devices found!");
            print_info("Make sure Bluetooth is enabled and devices are discoverable.");
        } else {
            print_device_table(devices, device_count);
        }

        print_connection_menu();
        
        // Get user input
        printf("\n%sEnter your choice (1-%d, r=rescan, q=quit): %s", 
               COLOR_BLUE, device_count, COLOR_RESET);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            print_error("Input error");
            continue;
        }

        // Process input
        if (input[0] == 'q' || input[0] == 'Q') {
            break;
        } else if (input[0] == 'r' || input[0] == 'R') {
            continue;
        } else if (input[0] == 's' || input[0] == 'S') {
            check_bluetooth_hardware();
            printf("\n%sPress Enter to continue...%s", COLOR_YELLOW, COLOR_RESET);
            getchar();
            continue;
        }

        int num = atoi(input);
        if (num < 1 || num > device_count) {
            print_error("Invalid device number!");
            sleep(1);
            continue;
        }

        // Attempt connection
        clear_screen();
        print_header("CONNECTION STATUS");
        printf("\n%sAttempting to connect to:%s\n", COLOR_CYAN, COLOR_RESET);
        printf("  Name:    %s%s%s\n", COLOR_GREEN, devices[num-1].name, COLOR_RESET);
        printf("  Address: %s%s%s\n", COLOR_GREEN, devices[num-1].addr, COLOR_RESET);
        
        int conn = connect_device(devices[num-1].addr);
        if (conn >= 0) {
            print_success("\nConnection established successfully!");
            close(conn);
        } else {
            print_error("\nFailed to connect to device");
            printf("  Error: %s%s%s\n", COLOR_RED, strerror(errno), COLOR_RESET);
        }

        printf("\n%sPress Enter to continue...%s", COLOR_YELLOW, COLOR_RESET);
        getchar(); // Clear the newline
        getchar(); // Wait for Enter
    }

    clear_screen();
    print_centered("Bluetooth Device Manager terminated");
    return 0;
}

// [Previous functions like check_bluetooth_hardware, ensure_bluetooth_enabled, etc. would go here]
// [Add all the enhanced versions of your existing functions]

// Enhanced scan_devices function
int scan_devices(bluetooth_device *devices, int *count) {
    inquiry_info *ii = NULL;
    int max_rsp = MAX_DEVICES;
    int num_rsp;
    int dev_id, sock;
    int flags = IREQ_CACHE_FLUSH;
    char addr[19] = { 0 };
    char name[249] = { 0 };

    // Get the first available Bluetooth adapter
    if ((dev_id = hci_get_route(NULL)) < 0) {
        print_error("No Bluetooth adapter found");
        check_bluetooth_hardware();
        return -1;
    }

    // Ensure Bluetooth is enabled
    if (ensure_bluetooth_enabled(dev_id) < 0) {
        return -1;
    }

    // Open connection to the Bluetooth adapter
    if ((sock = hci_open_dev(dev_id)) < 0) {
        print_error("Cannot open Bluetooth adapter");
        return -1;
    }

    // Get paired devices first
    if (get_paired_devices(dev_id, devices, count) < 0) {
        print_warning("Could not get paired devices");
    }

    // Allocate memory for inquiry results
    if ((ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info))) == NULL) {
        print_error("Memory allocation failed");
        close(sock);
        return -1;
    }

    print_info("Scanning for nearby Bluetooth devices...");
    
    // Perform Bluetooth device discovery
    if ((num_rsp = hci_inquiry(dev_id, SCAN_TIME, max_rsp, NULL, &ii, flags)) < 0) {
        print_error("Device discovery failed");
        free(ii);
        close(sock);
        return *count;  // Return count of paired devices if any
    }

    // Store found devices
    for (int i = 0; i < num_rsp && *count < MAX_DEVICES; i++) {
        ba2str(&(ii+i)->bdaddr, addr);
        
        // Skip if already listed
        int exists = 0;
        for (int j = 0; j < *count; j++) {
            if (strcmp(devices[j].addr, addr) == 0) {
                exists = 1;
                break;
            }
        }
        if (exists) continue;

        // Get device info
        strcpy(devices[*count].addr, addr);
        if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name)-1, name, 0) < 0) {
            strcpy(name, "[unknown]");
        }
        strcpy(devices[*count].name, name);
        devices[*count].is_paired = 0;
        
        // Get RSSI (signal strength)
        int8_t rssi;
        if (hci_read_rssi(sock, htobs(dev_id), &rssi, 1000) == 0) {
            devices[*count].rssi = rssi;
        } else {
            devices[*count].rssi = 0;
        }
        
        // Get device class
        if (hci_read_class_of_dev(sock, devices[*count].dev_class, 1000) < 0) {
            memset(devices[*count].dev_class, 0, 3);
        }
        
        (*count)++;
    }

    free(ii);
    close(sock);
    return *count;
}

// Enhanced connect_device function
int connect_device(const char *addr) {
    struct sockaddr_rc addr_rc = { 0 };
    struct sockaddr_l2 addr_l2 = { 0 };
    int sock = -1;
    int retries = 3;
    
    // Try RFCOMM first
    while (retries-- > 0) {
        if ((sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0) {
            print_error("RFCOMM socket creation failed");
            continue;
        }

        addr_rc.rc_family = AF_BLUETOOTH;
        addr_rc.rc_channel = 1;  // Default RFCOMM channel
        str2ba(addr, &addr_rc.rc_bdaddr);

        if (connect(sock, (struct sockaddr *)&addr_rc, sizeof(addr_rc)) == 0) {
            return sock;
        }
        
        close(sock);
        sleep(1); // Wait before retry
    }

    // If RFCOMM fails, try L2CAP
    retries = 3;
    while (retries-- > 0) {
        if ((sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0) {
            print_error("L2CAP socket creation failed");
            continue;
        }

        addr_l2.l2_family = AF_BLUETOOTH;
        addr_l2.l2_psm = htobs(0x1001);
        str2ba(addr, &addr_l2.l2_bdaddr);

        if (connect(sock, (struct sockaddr *)&addr_l2, sizeof(addr_l2)) == 0) {
            return sock;
        }
        
        close(sock);
        sleep(1); // Wait before retry
    }

    return -1;
}

// [Additional helper functions for UI and error handling]