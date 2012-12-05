#include <stdio.h>
#include <err.h>
#include "errors.h"

void wc(FILE *);
int is_whitespace(int);

int main(int argc, const char *argv[])
{
    // If no command line argument is given, read from stdin
    if (argc == 1) {
        wc(stdin);
        return 0;
    }

    // Otherwise open the given file (further arguments are ignored)
    FILE *stream = fopen(argv[1], "r");
    if (stream == NULL) {
        err(ARG_ERROR, "Cannot open %s for reading", argv[1]);
    }

    // Count
    wc(stream);

    // Close the file
    if (fclose(stream) == EOF) {
        err(INPUT_ERROR, "Error closing file");
    }

    return 0;
}

/*
 * Reads characters from *stream and prints the number of newlines, words and
 * bytes read until EOF. A word is a sequence of bytes not containing a blank,
 * a tab or a newline (following the K&R in this).
 */
void wc(FILE *stream)
{
    int newline_cnt = 0;
    int word_cnt    = 0;
    int byte_cnt    = 0;

    int in_word = 0;

    // Read the stream character by character
    int cur_char;
    while ((cur_char = fgetc(stream)) != EOF) {
        // Increment the number of bytes read for every character
        ++byte_cnt;

        // Increment the number of newlines/lines read for every newline (!)
        if (cur_char == '\n') {
            ++newline_cnt;
        }

        // If we haven't been in a word but encounter a word character now
        if (!in_word && !is_whitespace(cur_char)) {
            // Increment the number of words read
            ++word_cnt;

            // Say that we are in a word now
            in_word = 1;
        }
        // Say that we aren't in a word if we are on whitespace
        else if (is_whitespace(cur_char)) {
            in_word = 0;
        }
    }

    // Check whether our file walking ended abnormally
    if (ferror(stream)) {
        err(OUTPUT_ERROR, "Error in reading");
    }

    // Print the summary of counts
    printf("%6d %6d %6d\n", newline_cnt, word_cnt, byte_cnt);
}

/*
 * Return 1 if the specified character is regarded whitespace and 0 otherwise.
 * Newlines, tabs and blanks are regarded whitespace.
 */
int is_whitespace(int character)
{
    if (character == '\n' || character == ' ' || character == '\t') {
        return 1;
    }

    return 0;
}
