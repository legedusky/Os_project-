#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_WORD 10000

// A small struct to store word-frequency
typedef struct {
    char word[100];
    int count;
} WordFreq;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: summarize <filename>\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("summarize");
        return 1;
    }

    char line[MAX_LINE];
    int line_count = 0;
    int word_count = 0;
    int longest_line_length = 0;

    WordFreq words[MAX_WORD];
    int word_index = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_count++;

        int len = strlen(line);
        if (len > longest_line_length)
            longest_line_length = len;

        // Tokenize line into words
        char *token = strtok(line, " \t\n\r.,!?;:\"()[]{}<>");

        while (token) {
            // Convert to lowercase for uniform counting
            for (char *p = token; *p; ++p) *p = tolower(*p);
            word_count++;

            // Search if word exists already
            int found = 0;
            for (int i = 0; i < word_index; i++) {
                if (strcmp(words[i].word, token) == 0) {
                    words[i].count++;
                    found = 1;
                    break;
                }
            }

            // If not found, add new word
            if (!found && word_index < MAX_WORD) {
                strncpy(words[word_index].word, token, sizeof(words[word_index].word) - 1);
                words[word_index].word[sizeof(words[word_index].word) - 1] = '\0';
                words[word_index].count = 1;
                word_index++;
            }

            token = strtok(NULL, " \t\n\r.,!?;:\"()[]{}<>");
        }
    }

    fclose(fp);

    // Find the most frequent word
    int max_freq = 0;
    char most_freq_word[100] = "";

    for (int i = 0; i < word_index; i++) {
        if (words[i].count > max_freq) {
            max_freq = words[i].count;
            strcpy(most_freq_word, words[i].word);
        }
    }

    // Print summary
    printf("Lines           : %d\n", line_count);
    printf("Words           : %d\n", word_count);
    printf("Longest line    : %d characters\n", longest_line_length);
    if (max_freq > 0) {
        printf("Most frequent   : '%s' (%d times)\n", most_freq_word, max_freq);
    } else {
        printf("Most frequent   : N/A\n");
    }

    return 0;
}
