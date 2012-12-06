#include <stdio.h>
#include <string.h>
#include <err.h>
#include "errors.h"

void pseudo_grep(const char *, int, FILE *);

int main(int argc, const char *argv[])
{
    // Check command line arguments
    if (argc == 1) {
        errx(ARG_ERROR, "There must be at least two command line arguments");
    }

    const char *pattern  = argv[1];
    int pat_length       = strlen(pattern);
        // Not quite from stdio.h, but not part of the task

    // If we have just the pattern as argument
    if (argc == 2) {
        // Search stdin
        pseudo_grep(pattern, pat_length, stdin);
    }

    // Otherwise open the given file (further arguments are ignored)
    FILE *stream = fopen(argv[2], "r");
    if (stream == NULL) {
        err(ARG_ERROR, "Cannot open %s for reading", argv[2]);
    }

    // Search
    pseudo_grep(pattern, pat_length, stream);

    // Close the file
    if (fclose(stream) == EOF) {
        err(INPUT_ERROR, "Error closing file");
    }

    return 0;
}

/*
 * Prints every line of *stream containing pattern to stdout.
 */
void pseudo_grep(const char pattern[], int length, FILE *stream)
{
    /*
     * The stream is read character by character (byte by byte to be precise)
     * and compared to the first character of pattern. If the two characters
     * match, the next character read is compared to the pattern's second
     * character and so on.
     *     If there are length successive matches, the input
     * is wound back to the last newline and printed until the next newline.
     *     If there are less than length successive matches before a mismatch,
     * we start looking for pattern's first character again after the position
     * of the first match.
     *
     * The behaviour for patterns containing \n is undefined.
     */
    long from_linestart = 0;
    int matched_chars   = 0;

    // Read the stream character by character
    int cur_char;
    while ((cur_char = fgetc(stream)) != EOF) {
        // Increment number of characters read since last \n
        ++from_linestart;

        // Reset that number if we're at the end of a line
        if (cur_char == '\n') {
            from_linestart = 0;
        }

        // If the current character matches the next character from pattern
        if (cur_char == pattern[matched_chars]) {
            // Increment the number of characters matched so far
            ++matched_chars;

            // If we have the whole pattern matched
            if (matched_chars == length) {
                // Go back to the beginning of the line
                if (fseek(stream, -from_linestart, SEEK_CUR) == -1) {
                    printf("%d", (int) from_linestart);
                    err(INPUT_ERROR, "Cannot go back to last newline");
                }

                // Read characters
                int line_char;
                while ((line_char = fgetc(stream)) != EOF) {
                    // Print the to standard output
                    if (fputc(line_char, stdout) == EOF) {
                        err(OUTPUT_ERROR, "Error writing to stdout");
                    }

                    // Abort printing on the end of the line
                    if (line_char == '\n') {
                        break;
                    }
                }
                if (ferror(stream)) {
                    err(INPUT_ERROR, "Error in reading");
                }

                // Reset the pattern pointer and the position in the line
                matched_chars  = 0;
                from_linestart = 0;
            }
        }
        // If we have a mismatch after some characters of pattern matched
        else if (cur_char != pattern[matched_chars] && matched_chars > 0) {
            // Rewind to the position after the first matching character
            if (fseek(stream, -matched_chars, SEEK_CUR) == -1) {
                err(INPUT_ERROR, "Cannot go back in input stream");
            }

            // Reset number of characters matched and line offset
            from_linestart -= matched_chars;
            matched_chars   = 0;
        }
    }
    if (ferror(stream)) {
        err(INPUT_ERROR, "Error in reading");
    }
}
