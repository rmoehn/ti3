#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define BUFFSIZE 4096
#define TRASHDIRMODE 0700
#define TRASHDIRNAME ".ti3_trash"

/* You'd better not read (nor use) this program. */

int filecopy(const char *, const char *);
char *filecat(char *, const char *, const char *);
int filecatlen(const char *, const char *);
char *get_trashname(char *, const char *);
int get_trashnamelen(const char *);
void opt_mktrashdir();
void move_to_trash(const char *);
void recover_from_trash(const char *);
void finally_delete(const char *);

int main(int argc, char *argv[])
{
    char *prog = argv[0];

    /* Check arguments */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mode> <filename>\n", prog);
        return 1;
    }

    /* Look at the first command-line argument and decide what to do. */
    switch (argv[1][1]) {
        case 'p':
            move_to_trash(argv[2]);
            break;
        case 'g':
            recover_from_trash(argv[2]);
            break;
        case 'r':
            finally_delete(argv[2]);
            break;
        default:
            errx(1, "Unknown option %s.", argv[1]);
    }

    return 0;
}

/* Moves the file with the given name to the trash. */
void move_to_trash(const char *filename)
{
    /* Create the trash if it doesn't exist already. */
    opt_mktrashdir();

    /* Obtain the absolute path to the file in the trash */
    char trashname[get_trashnamelen(filename)];
    get_trashname(trashname, filename);

    /* Copy the file over */
    if (filecopy(filename, trashname) != 0) {
        err(2, "Error copying %s to %s.", filename, trashname);
    }

    /* Remove the original file */
    if (unlink(filename) == -1) {
        err(3, "Cannot remove source file %s.", filename);
    }
}

/* Moves the given file from the trash to the current directory. */
void recover_from_trash(const char *filename)
{
    /* Obtain the name of the file in the trash */
    char trashname[get_trashnamelen(filename)];
    get_trashname(trashname, filename);

    /* Copy the file to the current directory */
    if (filecopy(trashname, filename) != 0) {
        err(2, "Error copying %s to current directory.", filename);
    }

    /* Remove the original file */
    if (unlink(trashname) == -1) {
        err(3, "Cannot remove file %s from trash.", trashname);
    }
}

/* Ultimately removes the given file from the trash. */
void finally_delete(const char *filename)
{
    /* Obtain the name of the file in the trash */
    char trashname[get_trashnamelen(filename)];
    get_trashname(trashname, filename);

    /* Remove it */
    if (unlink(trashname) == -1) {
        err(3, "Cannot remove file %s from trash.", trashname);
    }
}

/*
 * Copy the contents of a file into another not yet existing file.
 *
 * Return 0 on success.
 * Return 1 on problems with the input file.
 * Return 2 on problems with the output file.
 * Return 3 on problems in the copying process.
 */
int filecopy(const char *infile, const char *outfile)
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

    /* Close the two files */
    if (close(in_fd) == -1) {
        return 1;
    }
    if (close(out_fd) == -1) {
        return 2;
    }

    return 0;
}

/* Creates the trash directory if it does not exist yet.  */
void opt_mktrashdir()
{
    /* Obtain the complete path of the trash directory */
    char trashdirname[get_trashnamelen("")]; /* "" is trash only */
    get_trashname(trashdirname, "");
    if (trashdirname == NULL) {
        err(
            4,
            "Cannot obtain a path to the trash directory %s.",
            trashdirname
        );
    }

    /* Create the directory if it doesn't exist already */
    if (mkdir(trashdirname, TRASHDIRMODE) != 0) {
        if (errno != EEXIST) {
            err(5, "Cannot create trash directory %s.", trashdirname);
        }
    }
}

/*
 * Returns the string length (including \0) of the absolute path of the given
 * file in the trash.
 */
int get_trashnamelen(const char *filename) {
    /* Obtain the current home directory */
    const char *homedir = getenv("HOME");
    if (homedir == NULL) {
        return 1;
    }

    return strlen(homedir) + strlen(TRASHDIRNAME) + strlen(filename) + 3;
}

/*
 * From a given filename, constructs its absolute path in the trash can.
 */
char *get_trashname(char *outfilename, const char *filename)
{
    /* Obtain the current home directory */
    const char *homedir = getenv("HOME");
    if (homedir == NULL) {
        return NULL;
    }

    /* Creates the absolute path to the trash directory */
    char trashdir[filecatlen(homedir, TRASHDIRNAME)];
    filecat(trashdir, homedir, TRASHDIRNAME);

    return filecat(outfilename, trashdir, filename);
}

/*
 * Returns the string length of a path to be constructed by filecat.
 */
int filecatlen(const char *filename1, const char *filename2)
{
    return strlen(filename1) + strlen(filename2) + 2;
}

/*
 * Joins two filenames with a /.
 */
char *filecat(char *outfilename, const char *filename1, const char *filename2)
{
    strcpy(outfilename, filename1);
    strcat(outfilename, "/");
    strcat(outfilename, filename2);

    return outfilename;
}
