#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>

#define MAX_NOTE_LENGTH 4096
#define MAX_LINE_LENGTH 1024
#define MAX_PATH_LENGTH 512
#define MAX_CATEGORY_LENGTH 64
#define MAX_SEARCH_RESULTS 100
#define DEFAULT_CATEGORY "general"

// Structure to store note information
typedef struct {
    char filepath[MAX_PATH_LENGTH];
    char category[MAX_CATEGORY_LENGTH];
    time_t timestamp;
    char date_str[20];  // YYYY-MM-DD format
    char time_str[20];  // HH-MM-SS format
} NoteInfo;

// Structure for search results
typedef struct {
    char filepath[MAX_PATH_LENGTH];
    char category[MAX_CATEGORY_LENGTH];
    char date_str[20];
    char time_str[20];
    char line[MAX_LINE_LENGTH];
    int line_num;
} SearchResult;

/**
 * Get the path to the notes directory
 * Creates the directory if it doesn't exist
 */
char* get_notes_dir() {
    char* home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "quicknote: Could not find HOME directory\n");
        return NULL;
    }
    
    static char notes_dir[MAX_PATH_LENGTH];
    snprintf(notes_dir, MAX_PATH_LENGTH, "%s/.quicknotes", home_dir);
    
    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(notes_dir, &st) == -1) {
        if (mkdir(notes_dir, 0700) == -1) {
            fprintf(stderr, "quicknote: Could not create notes directory: %s\n", strerror(errno));
            return NULL;
        }
    }
    
    return notes_dir;
}

/**
 * Sanitize a string to be used as a category name
 */
void sanitize_category(const char* input, char* output, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < max_len - 1; i++) {
        if (isalnum(input[i]) || input[i] == '_' || input[i] == '-') {
            output[j++] = tolower(input[i]);
        }
    }
    output[j] = '\0';
    
    // Use default category if sanitized string is empty
    if (j == 0) {
        strncpy(output, DEFAULT_CATEGORY, max_len - 1);
        output[max_len - 1] = '\0';
    }
}

/**
 * Get the path to a category directory
 * Creates the directory if it doesn't exist
 */
char* get_category_dir(const char* category) {
    char* notes_dir = get_notes_dir();
    if (notes_dir == NULL) return NULL;
    
    // Sanitize category name
    char sanitized_category[MAX_CATEGORY_LENGTH];
    sanitize_category(category, sanitized_category, MAX_CATEGORY_LENGTH);
    
    static char category_dir[MAX_PATH_LENGTH];
    snprintf(category_dir, MAX_PATH_LENGTH, "%s/%s", notes_dir, sanitized_category);
    
    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(category_dir, &st) == -1) {
        if (mkdir(category_dir, 0700) == -1) {
            fprintf(stderr, "quicknote: Could not create category directory: %s\n", strerror(errno));
            return NULL;
        }
        printf("Created new category: %s\n", sanitized_category);
    }
    
    return category_dir;
}

/**
 * Save a new note with the current timestamp
 */
int save_note(const char* category, const char* note_text) {
    // Get category directory
    char* category_dir = get_category_dir(category);
    if (category_dir == NULL) return 1;
    
    // Get current time
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    // Create filename with timestamp
    char filename[MAX_PATH_LENGTH];
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H-%M-%S", tm_info);
    snprintf(filename, MAX_PATH_LENGTH, "%s/note_%s.txt", category_dir, time_str);
    
    // Open file for writing
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "quicknote: Could not create note file: %s\n", strerror(errno));
        return 1;
    }
    
    // Write timestamp and category header and note content
    char header[256];
    strftime(header, sizeof(header), "=== Note created on %Y-%m-%d at %H:%M:%S ===\n", tm_info);
    fputs(header, file);
    
    // Sanitize category for display
    char sanitized_category[MAX_CATEGORY_LENGTH];
    sanitize_category(category, sanitized_category, MAX_CATEGORY_LENGTH);
    fprintf(file, "=== Category: %s ===\n\n", sanitized_category);
    
    fputs(note_text, file);
    
    // Add a newline at the end if the note doesn't end with one
    if (note_text[0] != '\0' && note_text[strlen(note_text) - 1] != '\n') {
        fputc('\n', file);
    }
    
    fclose(file);
    printf("Note saved to category '%s'\n", sanitized_category);
    return 0;
}

/**
 * Extract date and time from filename
 */
void extract_datetime_from_filename(const char* filename, char* date_str, char* time_str) {
    // Extract from note_YYYY-MM-DD_HH-MM-SS.txt
    if (sscanf(filename, "note_%[^_]_%[^.]", date_str, time_str) != 2) {
        strcpy(date_str, "????-??-??");
        strcpy(time_str, "??-??-??");
    }
    
    // Replace underscores with dashes
    for (char* p = date_str; *p; p++) {
        if (*p == '_') *p = '-';
    }
    
    for (char* p = time_str; *p; p++) {
        if (*p == '_') *p = '-';
    }
}

/**
 * Compare function for sorting notes by timestamp (newest first)
 */
int compare_notes_timestamp(const void* a, const void* b) {
    const NoteInfo* note_a = (const NoteInfo*)a;
    const NoteInfo* note_b = (const NoteInfo*)b;
    
    // Sort in descending order (newest first)
    if (note_a->timestamp > note_b->timestamp) return -1;
    if (note_a->timestamp < note_b->timestamp) return 1;
    return 0;
}

/**
 * List all available categories
 */
int list_categories() {
    char* notes_dir = get_notes_dir();
    if (notes_dir == NULL) return 1;
    
    DIR* dir = opendir(notes_dir);
    if (dir == NULL) {
        fprintf(stderr, "quicknote: Could not open notes directory: %s\n", strerror(errno));
        return 1;
    }
    
    printf("=== Available Categories ===\n");
    int count = 0;
    struct dirent* entry;
    
    // Array to store category info
    struct {
        char name[MAX_CATEGORY_LENGTH];
        int note_count;
    } categories[100];  // Limit to 100 categories
    int cat_count = 0;
    
    while ((entry = readdir(dir)) != NULL && cat_count < 100) {
        // Skip . and .. entries and any hidden files
        if (entry->d_name[0] == '.') continue;
        
        // Only count directories
        char path[MAX_PATH_LENGTH];
        snprintf(path, MAX_PATH_LENGTH, "%s/%s", notes_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            // Count notes in this category
            DIR* cat_dir = opendir(path);
            if (cat_dir != NULL) {
                int note_count = 0;
                struct dirent* note_entry;
                while ((note_entry = readdir(cat_dir)) != NULL) {
                    if (strstr(note_entry->d_name, "note_") == note_entry->d_name) {
                        note_count++;
                    }
                }
                closedir(cat_dir);
                
                // Store category info
                strncpy(categories[cat_count].name, entry->d_name, MAX_CATEGORY_LENGTH - 1);
                categories[cat_count].name[MAX_CATEGORY_LENGTH - 1] = '\0';
                categories[cat_count].note_count = note_count;
                cat_count++;
            }
        }
    }
    
    closedir(dir);
    
    // Sort categories alphabetically
    for (int i = 0; i < cat_count - 1; i++) {
        for (int j = i + 1; j < cat_count; j++) {
            if (strcmp(categories[i].name, categories[j].name) > 0) {
                // Swap
                char temp_name[MAX_CATEGORY_LENGTH];
                strncpy(temp_name, categories[i].name, MAX_CATEGORY_LENGTH);
                strncpy(categories[i].name, categories[j].name, MAX_CATEGORY_LENGTH);
                strncpy(categories[j].name, temp_name, MAX_CATEGORY_LENGTH);
                
                int temp_count = categories[i].note_count;
                categories[i].note_count = categories[j].note_count;
                categories[j].note_count = temp_count;
            }
        }
    }
    
    // Display categories
    for (int i = 0; i < cat_count; i++) {
        printf("- %s (%d notes)\n", categories[i].name, categories[i].note_count);
        count++;
    }
    
    if (count == 0) {
        printf("No categories found. Create one by adding a note with -c option.\n");
    }
    
    return 0;
}

/**
 * List notes in a specific category or all categories
 */
 int list_notes(const char* category) {
    char* notes_dir = get_notes_dir();
    if (notes_dir == NULL) return 1;
    
    NoteInfo* notes = NULL;
    int note_count = 0;
    int max_notes = 1000;  // Maximum number of notes to keep track of
    
    // Allocate memory for notes array
    notes = (NoteInfo*)malloc(max_notes * sizeof(NoteInfo));
    if (notes == NULL) {
        fprintf(stderr, "quicknote: Memory allocation failed\n");
        return 1;
    }
    
    if (category == NULL || strcmp(category, "all") == 0) {
        // List notes from all categories
        DIR* dir = opendir(notes_dir);
        if (dir == NULL) {
            fprintf(stderr, "quicknote: Could not open notes directory: %s\n", strerror(errno));
            free(notes);
            return 1;
        }
        
        printf("=== Recent Notes (All Categories) ===\n");
        struct dirent* entry;
        
        while ((entry = readdir(dir)) != NULL) {
            // Skip . and .. entries and any hidden files
            if (entry->d_name[0] == '.') continue;
            
            // Only process directories
            char path[MAX_PATH_LENGTH];
            snprintf(path, MAX_PATH_LENGTH, "%s/%s", notes_dir, entry->d_name);
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                // Get note files in this category
                DIR* cat_dir = opendir(path);
                if (cat_dir != NULL) {
                    struct dirent* note_entry;
                    while ((note_entry = readdir(cat_dir)) != NULL && note_count < max_notes) {
                        if (strstr(note_entry->d_name, "note_") == note_entry->d_name) {
                            // Get note info
                            char note_path[MAX_PATH_LENGTH];
                            snprintf(note_path, MAX_PATH_LENGTH, "%s/%s", path, note_entry->d_name);
                            
                            struct stat note_st;
                            if (stat(note_path, &note_st) == 0) {
                                strncpy(notes[note_count].filepath, note_path, MAX_PATH_LENGTH - 1);
                                notes[note_count].filepath[MAX_PATH_LENGTH - 1] = '\0';
                                
                                strncpy(notes[note_count].category, entry->d_name, MAX_CATEGORY_LENGTH - 1);
                                notes[note_count].category[MAX_CATEGORY_LENGTH - 1] = '\0';
                                
                                notes[note_count].timestamp = note_st.st_mtime;
                                
                                extract_datetime_from_filename(note_entry->d_name, 
                                                            notes[note_count].date_str,
                                                            notes[note_count].time_str);
                                
                                note_count++;
                            }
                        }
                    }
                    closedir(cat_dir);
                }
            }
        }
        
        closedir(dir);
    } else {
        // List notes from a specific category
        char* category_dir = get_category_dir(category);
        if (category_dir == NULL) {
            free(notes);
            return 1;
        }
        
        DIR* dir = opendir(category_dir);
        if (dir == NULL) {
            fprintf(stderr, "quicknote: Could not open category directory: %s\n", strerror(errno));
            free(notes);
            return 1;
        }
        
        printf("=== Recent Notes in Category '%s' ===\n", category);
        struct dirent* entry;
        
        while ((entry = readdir(dir)) != NULL && note_count < max_notes) {
            if (strstr(entry->d_name, "note_") == entry->d_name) {
                // Get note info
                char note_path[MAX_PATH_LENGTH];
                snprintf(note_path, MAX_PATH_LENGTH, "%s/%s", category_dir, entry->d_name);
                
                struct stat note_st;
                if (stat(note_path, &note_st) == 0) {
                    strncpy(notes[note_count].filepath, note_path, MAX_PATH_LENGTH - 1);
                    notes[note_count].filepath[MAX_PATH_LENGTH - 1] = '\0';
                    
                    strncpy(notes[note_count].category, category, MAX_CATEGORY_LENGTH - 1);
                    notes[note_count].category[MAX_CATEGORY_LENGTH - 1] = '\0';
                    
                    notes[note_count].timestamp = note_st.st_mtime;
                    
                    extract_datetime_from_filename(entry->d_name, 
                                                  notes[note_count].date_str,
                                                  notes[note_count].time_str);
                    
                    note_count++;
                }
            }
        }
        
        closedir(dir);
    }
    
    if (note_count == 0) {
        printf("No notes found.\n");
        free(notes);
        return 0;
    }
    
    // Sort notes by timestamp (newest first)
    qsort(notes, note_count, sizeof(NoteInfo), compare_notes_timestamp);
    
    // Display notes (limit to 25 most recent)
    int display_count = (note_count > 25) ? 25 : note_count;
    for (int i = 0; i < display_count; i++) {
        printf("\n%s %s  [%s]\n", notes[i].date_str, notes[i].time_str, notes[i].category);
        
        // Open the note file and print the first few lines of content
        FILE* file = fopen(notes[i].filepath, "r");
        if (file) {
            char line[MAX_LINE_LENGTH];
            int lines_printed = 0;
            
            // Skip the header lines (timestamp and category)
            fgets(line, sizeof(line), file); // Skip timestamp line
            fgets(line, sizeof(line), file); // Skip category line
            fgets(line, sizeof(line), file); // Skip empty line
            
            // Print the first 3 lines of actual content
            while (fgets(line, sizeof(line), file) && lines_printed < 3) {
                // Remove trailing newline if present
                line[strcspn(line, "\n")] = '\0';
                printf("  %s\n", line);
                lines_printed++;
            }
            
            // If there's more content, indicate it
            if (fgets(line, sizeof(line), file)) {
                printf("  ... (more content)\n");
            }
            
            fclose(file);
        }
    }
    
    free(notes);
    return 0;
}

/**
 * Search through notes for a specific term
 */
int search_notes(const char* category, const char* search_term) {
    if (search_term == NULL || strlen(search_term) == 0) {
        fprintf(stderr, "quicknote: Search term cannot be empty\n");
        return 1;
    }
    
    char* notes_dir = get_notes_dir();
    if (notes_dir == NULL) return 1;
    
    // Array to store search results
    SearchResult* results = (SearchResult*)malloc(MAX_SEARCH_RESULTS * sizeof(SearchResult));
    if (results == NULL) {
        fprintf(stderr, "quicknote: Memory allocation failed\n");
        return 1;
    }
    int result_count = 0;
    
    if (category == NULL || strcmp(category, "all") == 0) {
        printf("=== Search Results for '%s' (All Categories) ===\n", search_term);
        
        // Search in all categories
        DIR* dir = opendir(notes_dir);
        if (dir == NULL) {
            fprintf(stderr, "quicknote: Could not open notes directory: %s\n", strerror(errno));
            free(results);
            return 1;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip . and .. entries and any hidden files
            if (entry->d_name[0] == '.') continue;
            
            // Only process directories
            char path[MAX_PATH_LENGTH];
            snprintf(path, MAX_PATH_LENGTH, "%s/%s", notes_dir, entry->d_name);
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                // Search note files in this category
                DIR* cat_dir = opendir(path);
                if (cat_dir != NULL) {
                    struct dirent* note_entry;
                    while ((note_entry = readdir(cat_dir)) != NULL && result_count < MAX_SEARCH_RESULTS) {
                        if (strstr(note_entry->d_name, "note_") == note_entry->d_name) {
                            // Search this note
                            char note_path[MAX_PATH_LENGTH];
                            snprintf(note_path, MAX_PATH_LENGTH, "%s/%s", path, note_entry->d_name);
                            
                            FILE* file = fopen(note_path, "r");
                            if (file) {
                                char line[MAX_LINE_LENGTH];
                                int line_num = 0;
                                while (fgets(line, sizeof(line), file) && result_count < MAX_SEARCH_RESULTS) {
                                    line_num++;
                                    // Simple case-insensitive search
                                    char* line_lower = strdup(line);
                                    if (line_lower) {
                                        for (int i = 0; line_lower[i]; i++) {
                                            line_lower[i] = tolower(line_lower[i]);
                                        }
                                        
                                        char* search_lower = strdup(search_term);
                                        if (search_lower) {
                                            for (int i = 0; search_lower[i]; i++) {
                                                search_lower[i] = tolower(search_lower[i]);
                                            }
                                            
                                            if (strstr(line_lower, search_lower)) {
                                                // Found a match
                                                strncpy(results[result_count].filepath, note_path, MAX_PATH_LENGTH - 1);
                                                results[result_count].filepath[MAX_PATH_LENGTH - 1] = '\0';
                                                
                                                strncpy(results[result_count].category, entry->d_name, MAX_CATEGORY_LENGTH - 1);
                                                results[result_count].category[MAX_CATEGORY_LENGTH - 1] = '\0';
                                                
                                                extract_datetime_from_filename(note_entry->d_name, 
                                                                            results[result_count].date_str,
                                                                            results[result_count].time_str);
                                                
                                                // Store the matching line
                                                strncpy(results[result_count].line, line, MAX_LINE_LENGTH - 1);
                                                results[result_count].line[MAX_LINE_LENGTH - 1] = '\0';
                                                results[result_count].line_num = line_num;
                                                
                                                result_count++;
                                            }
                                            free(search_lower);
                                        }
                                        free(line_lower);
                                    }
                                }
                                fclose(file);
                            }
                        }
                    }
                    closedir(cat_dir);
                }
            }
        }
        
        closedir(dir);
    } else {
        // Search in specific category
        char* category_dir = get_category_dir(category);
        if (category_dir == NULL) {
            free(results);
            return 1;
        }
        
        printf("=== Search Results for '%s' in Category '%s' ===\n", search_term, category);
        
        DIR* dir = opendir(category_dir);
        if (dir == NULL) {
            fprintf(stderr, "quicknote: Could not open category directory: %s\n", strerror(errno));
            free(results);
            return 1;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL && result_count < MAX_SEARCH_RESULTS) {
            if (strstr(entry->d_name, "note_") == entry->d_name) {
                // Search this note
                char note_path[MAX_PATH_LENGTH];
                snprintf(note_path, MAX_PATH_LENGTH, "%s/%s", category_dir, entry->d_name);
                
                FILE* file = fopen(note_path, "r");
                if (file) {
                    char line[MAX_LINE_LENGTH];
                    int line_num = 0;
                    while (fgets(line, sizeof(line), file) && result_count < MAX_SEARCH_RESULTS) {
                        line_num++;
                        // Simple case-insensitive search
                        char* line_lower = strdup(line);
                        if (line_lower) {
                            for (int i = 0; line_lower[i]; i++) {
                                line_lower[i] = tolower(line_lower[i]);
                            }
                            
                            char* search_lower = strdup(search_term);
                            if (search_lower) {
                                for (int i = 0; search_lower[i]; i++) {
                                    search_lower[i] = tolower(search_lower[i]);
                                }
                                
                                if (strstr(line_lower, search_lower)) {
                                    // Found a match
                                    strncpy(results[result_count].filepath, note_path, MAX_PATH_LENGTH - 1);
                                    results[result_count].filepath[MAX_PATH_LENGTH - 1] = '\0';
                                    
                                    strncpy(results[result_count].category, category, MAX_CATEGORY_LENGTH - 1);
                                    results[result_count].category[MAX_CATEGORY_LENGTH - 1] = '\0';
                                    
                                    extract_datetime_from_filename(entry->d_name, 
                                                                  results[result_count].date_str,
                                                                  results[result_count].time_str);
                                    
                                    // Store the matching line
                                    strncpy(results[result_count].line, line, MAX_LINE_LENGTH - 1);
                                    results[result_count].line[MAX_LINE_LENGTH - 1] = '\0';
                                    results[result_count].line_num = line_num;
                                    
                                    result_count++;
                                }
                                free(search_lower);
                            }
                            free(line_lower);
                        }
                    }
                    fclose(file);
                }
            }
        }
        
        closedir(dir);
    }
    
    if (result_count == 0) {
        printf("No matches found.\n");
        free(results);
        return 0;
    }
    
    // Display results
    char prev_path[MAX_PATH_LENGTH] = "";
    for (int i = 0; i < result_count; i++) {
        // Print header for new file
        if (strcmp(prev_path, results[i].filepath) != 0) {
            printf("\n===== %s %s [Category: %s] =====\n", 
                   results[i].date_str, 
                   results[i].time_str, 
                   results[i].category);
            strncpy(prev_path, results[i].filepath, MAX_PATH_LENGTH);
            
            // Print context before and after match
            FILE* file = fopen(results[i].filepath, "r");
            if (file) {
                char line[MAX_LINE_LENGTH];
                int line_num = 0;
                int start_line = results[i].line_num - 2;
                int end_line = results[i].line_num + 2;
                
                if (start_line < 1) start_line = 1;
                
                // Print lines
                while (fgets(line, sizeof(line), file)) {
                    line_num++;
                    if (line_num >= start_line && line_num <= end_line) {
                        // Highlight the matching line
                        if (line_num == results[i].line_num) {
                            printf(">> %s", line);
                        } else {
                            printf("   %s", line);
                        }
                        
                        // Add newline if not present
                        if (line[0] != '\0' && line[strlen(line) - 1] != '\n') {
                            printf("\n");
                        }
                    }
                    if (line_num > end_line) break;
                }
                fclose(file);
            }
        }
        else {
            // Print just this match from the same file (no header/context)
            printf(">> %s", results[i].line);
            
            // Add newline if not present
            if (results[i].line[0] != '\0' && results[i].line[strlen(results[i].line) - 1] != '\n') {
                printf("\n");
            }
        }
    }
    
    free(results);
    return 0;
}

/**
 * Show usage instructions
 */
void print_usage() {
    printf("Usage:\n");
    printf("  quicknote \"Your note text here\"             - Create a note in default category\n");
    printf("  quicknote -c category \"Your note text\"      - Create a note in specific category\n");
    printf("  quicknote -l | --list                      - List recent notes (all categories)\n");
    printf("  quicknote -l category | --list category    - List notes in specific category\n");
    printf("  quicknote --categories                     - List all available categories\n");
    printf("  quicknote -s \"term\" | --search \"term\"     - Search all notes for term\n");
    printf("  quicknote -s \"term\" -c category           - Search notes in specific category\n");
    printf("  quicknote -h | --help                      - Show this help message\n");
}

/**
 * Join arguments into a single string
 */
char* join_args(int argc, char* argv[], int start, int end) {
    if (end == -1) end = argc;
    
    char* result = (char*)malloc(MAX_NOTE_LENGTH);
    if (result == NULL) {
        fprintf(stderr, "quicknote: Memory allocation failed\n");
        return NULL;
    }
    
    result[0] = '\0';
    size_t total_len = 0;
    
    for (int i = start; i < end; i++) {
        size_t arg_len = strlen(argv[i]);
        if (total_len + arg_len + 2 > MAX_NOTE_LENGTH) {
            fprintf(stderr, "quicknote: Note too long, truncating\n");
            break;
        }
        
        if (i > start) {
            strcat(result, " ");
            total_len++;
        }
        
        strcat(result, argv[i]);
        total_len += arg_len;
    }
    
    return result;
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {
    // Show usage if no arguments
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    // Process command flags
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }
    else if (strcmp(argv[1], "--categories") == 0) {
        return list_categories();
    }
    else if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
        if (argc > 2) {
            return list_notes(argv[2]);
        } else {
            return list_notes("all");
        }
    }
    else if (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "quicknote: Search term required\n");
            return 1;
        }
        
        // Check if we're searching in a specific category
        if (argc > 4 && (strcmp(argv[3], "-c") == 0 || strcmp(argv[3], "--category") == 0)) {
            char* search_term = join_args(argc, argv, 2, 3);
            if (search_term == NULL) return 1;
            
            int result = search_notes(argv[4], search_term);
            free(search_term);
            return result;
        } else {
            char* search_term = join_args(argc, argv, 2, -1);
            if (search_term == NULL) return 1;
            
            int result = search_notes("all", search_term);
            free(search_term);
            return result;
        }
    }
    else if (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--category") == 0) {
        if (argc < 4) {
            fprintf(stderr, "quicknote: Category name and note text required\n");
            return 1;
        }
        
        char* category = argv[2];
        char* note_text = join_args(argc, argv, 3, -1);
        if (note_text == NULL) return 1;
        
        int result = save_note(category, note_text);
        free(note_text);
        return result;
    }
    else {
        // Default case: save note with default category
        char* note_text = join_args(argc, argv, 1, -1);
        if (note_text == NULL) return 1;
        
        int result = save_note(DEFAULT_CATEGORY, note_text);
        free(note_text);
        return result;
    }
}