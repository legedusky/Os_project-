#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char filename[100];

    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.png", tm_info);

    printf("Press Enter to select the area for the screenshot...\n");
    getchar();  

    // Check if gnome-screenshot is available
    if (system("command -v gnome-screenshot > /dev/null") == 0) {
        char command[150];
        snprintf(command, sizeof(command), "gnome-screenshot -a -f %s", filename);  
        system(command);
        printf("Screenshot saved as %s using gnome-screenshot.\n", filename);
    }

    else {
        fprintf(stderr, "No screenshot tool found. Please install gnome-screenshot, scrot, or ImageMagick.\n");
        return 1;
    }

    return 0;
}