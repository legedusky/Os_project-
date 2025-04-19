#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void display_cpu_usage() {
    // This command uses 'top' to display CPU usage (1 second delay, show only the summary line)
    printf("CPU Usage:\n");
    system("top -bn1 | grep 'Cpu(s)'");
}

void display_running_processes() {
    // This command lists running processes with PID, CPU%, and MEM%
    printf("\nRunning Processes:\n");
    printf("PID\tCPU%\tMEM%\tCommand\n");

    // Use the 'ps' command to list processes and display their PID, CPU and memory usage
    system("ps -eo pid,%cpu,%mem,comm --sort=-%cpu | head -n 20");
}

int main() {
    printf("Fetching system information...\n");

    // Display CPU usage
    display_cpu_usage();

    // Display running processes
    display_running_processes();

    return 0;
}
