#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define WIFI_INTERFACE "wlp1s0" 

int main() {
    printf("Fixing Wi-Fi issues...\n\n");

    // 1. Restart the Network Manager
    printf("Restarting Network Manager...\n");
    int restart_status = system("sudo systemctl restart NetworkManager 2>/dev/null");

    if (restart_status != 0) {
        fprintf(stderr, "Failed to restart Network Manager.\n");
    }

    // 2. Flush DNS cache
    printf("Clearing DNS cache...\n");
    int dns_status = system("sudo resolvectl flush-caches 2>/dev/null");

    if (dns_status != 0) {
        fprintf(stderr, "Could not clear DNS cache (resolvectl not found).\n");
    }

    // 3. Wait for Wi-Fi to reconnect
    printf("Waiting for Wi-Fi to reconnect...\n");
    sleep(5); // wait a few seconds

    // Check Wi-Fi connection
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "nmcli device status | grep %s | grep connected > /dev/null", WIFI_INTERFACE);
    int wifi_status = system(cmd);

    if (wifi_status != 0) {
        fprintf(stderr, "Wi-Fi still not connected. Try manually reconnecting.\n");
        return 1;
    }

    printf("Wi-Fi connected. Checking internet connectivity...\n");

    // 4. Ping a known server
    int ping_status = system("ping -c 2 8.8.8.8 > /dev/null");

    if (ping_status == 0) {
        printf("Internet is working fine!\n");
    } else {
        fprintf(stderr, "Still having issues. Try restarting your router or checking cables.\n");
    }

    return 0;
}
