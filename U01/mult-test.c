#include <stdio.h>

#define MAXLINE 50

char *nextline(char *, FILE *);

/*
 * Usage:
 *     $ mult-test <filename>
 *
 * Read a text file line by line and checks whether the first line is the sum
 * of the products in the following lines. Print 'correct' if that is the
 * case, otherwise print 'incorrect'.
 *
 * Format of the input file:
 *
 *     -------------
 *     <int>
 *     <int> * <int>
 *     <int> * <int>
 *     ...
 *     -------------
 *
 *  - The first line contains a single integer.
 *  - All other lines contain two integers separated by " * ".
 *  - Anything not complying with these rules will cause the program to exit
 *    or to misbehave.
 */

int main(int argc, char *argv[])
{
    char *prog = argv[0];

    // Make sure there is exactly one command-line argument (the file name)
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", prog);
        return 1;
    }

    // Try and open the file
    char *filename = argv[1];
    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "%s: Can't open %s\n", prog, filename);
        return 2;
    }

    // Try and read the first line
    char sum_string[MAXLINE];
    if (nextline(sum_string, fp) == NULL) {
        fprintf(stderr, "%s: Given file %s is empty.\n", prog, filename);
        return 3;
    }

    // Read the contents to be the sum
    int expected_sum;
    if (sscanf(sum_string, "%d", &expected_sum) != 1) {
        fprintf(stderr, "%s: Illegal format in %s.\n", prog, filename);
        return 3;
    }

    // Read the remaining lines and sum them up
    int sum = 0;
    char prod_line[MAXLINE];
    while (nextline(prod_line, fp) != NULL) {
        int fac1, fac2;
        if (sscanf(prod_line, "%d * %d", &fac1, &fac2) != 2) {
            fprintf(stderr, "%s: Illegal format in %s.\n", prog, filename);
            return 3;
        }

        sum += fac1 * fac2;
    }

    // Print the result
    printf(sum == expected_sum ? "correct\n" : "incorrect\n");

    // Clean up
    if (fclose(fp) == EOF) {
        fprintf(stderr, "%s: Error closing file %s.\n", prog, filename);
    }

    return 0;
}

// A shorthand.
char *nextline(char *out_string, FILE *fp)
{
    return fgets(out_string, MAXLINE, fp);
}
