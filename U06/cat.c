#include <stdio.h>
#include <err.h>

#define INPUT_ERROR 1
#define OUTPUT_ERROR 2
#define ARG_ERROR 3

#define BUFSIZE 4096

void cat(FILE *);

/*
 * Reads characters from files or stdin if no file is specified at the command
 * line and prints them to stdout.
 */
int main(int argc, char *argv[])
{
    // If no command line arguments are specified, read from stdin
    if (argc == 1) {
        cat(stdin);
        return 0;
    }

    // Otherwise walk through all specified files
    for (int file_nr = 1; file_nr < argc; ++file_nr) {
        // Open the file
        FILE *stream = fopen(argv[file_nr], "r");
        if (stream == NULL) {
            err(ARG_ERROR, "Can't open %s for reading", argv[file_nr]);
        }

        // Write its contents to stdout
        cat(stream);

        // Close the file
        if (fclose(stream) == EOF) {
            err(INPUT_ERROR, "Error in closing file %s", argv[file_nr]);
        }
    }

    return 0;
}

/*
 * Reads characters from *stream and prints them to stdout until EOF is
 * encountered.
 */
void cat(FILE *stream)
{
    char buffer[BUFSIZE];

    // Read characters until an error or the end of the file occurs
    while (fgets(buffer, BUFSIZE, stream) != NULL) {
        // Print them to stdout
        if (fputs(buffer, stdout) == EOF) {
            err(OUTPUT_ERROR, "Error in writing to stdout");
        }
    }

    // Detect errors in reading
    if (ferror(stream)) {
        err(INPUT_ERROR, "Error while reading");
    }
}
