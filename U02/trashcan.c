#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

#define BUFFSIZE 4096
#define DIE(MSG, ARG) fprintf(stderr, "%s: " #MSG "\n", prog, ARG)

int filecopy(char *, char *);
int read(int, char *, int);
int write(int, char *, int);

int main(int argc, char *argv[])
{
    char *prog = argv[0];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mode> <filename>\n", prog);
        return 1;
    }

    if (filecopy(argv[1], argv[2]) != 0) {
        DIE("Problem in copying files %s", "bla");
    }

    return 0;
}

/*
 * Copy the contents of a file into another not yet existing file.
 *
 * Return 0 on success.
 * Return 1 on problems with the input file.
 * Return 2 on problems with the output file.
 * Return 3 on problems in the copying process.
 */
int filecopy(char *infile, char *outfile)
{
    /* Open the input file */
    int in_fd = open(infile, O_RDONLY, 0);
    if (in_fd == -1) {
        return 1;
    }

    /* Obtain the permissions of the input file */
    struct stat statbuf;
    if (fstat(in_fd, &statbuf) == -1) {
        return 1;
    }

    /* Create the output file with the same mode as the input file */
    int out_fd = open(outfile, O_WRONLY | O_CREAT | O_EXCL, statbuf.st_mode);
    if (out_fd == -1) {
        return 2;
    }

    /* Copy the file contents over */
    char buffer[BUFFSIZE];
    int n_read, n_written;
    while ((n_read = read(in_fd, buffer, BUFFSIZE)) > 0) {
        n_written = write(out_fd, buffer, n_read);
        assert(n_written == n_read);
    }
    if (n_read == -1) {
        return 3;
    }

    return 0;
}
