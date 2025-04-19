#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *fp;
    char path[128];
    char status[16];
    int capacity;

    // Read battery status
    fp = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (fp == NULL) {
        perror("Unable to read battery status");
        return 1;
    }
    fscanf(fp, "%15s", status);
    fclose(fp);

    // Read battery capacity
    fp = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (fp == NULL) {
        perror("Unable to read battery capacity");
        return 1;
    }
    fscanf(fp, "%d", &capacity);
    fclose(fp);

    printf("Battery Status: %s\n", status);
    printf("Charge Level: %d%%\n", capacity);

    return 0;
}
