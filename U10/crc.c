#include "errors.h"
#include <errno.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Here a different poly might be specified. Note that this implementation is
// only guaranteed to work with 16th-order polys. Thus the first component of
// the poly, (1 << 16), is implicit and the two numbers below must not be
// changed.
#define POLY 0 | (1 << 15) | (1 << 2) | (1 << 0)
#define POLY_ORD 16
#define CHECKSUM_BYTE_CNT 2

#define BUFSIZE 4096

void calculate(char *);
void validate(char *);
unsigned short calc_crc_rem(FILE *, long);
int next_bit_from(FILE *);

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 3) {
        errx(ERR_ARG, "Usage: crc [calculate|validate] <filename>");
    }

    // Dispatch, depending on first argument
    if (strcmp(argv[1], "calculate") == 0) {
        calculate(argv[2]);
    }
    else if (strcmp(argv[1], "validate") == 0) {
        validate(argv[2]);
    }
    else {
        errx(ERR_ARG, "Unknown mode: %s", argv[1]);
    }

    return 0;
}

void calculate(char *filename)
{
    // Append .crc to the file's name
    char new_filename[strlen(filename) + 4 + 1];
    sprintf(new_filename, "%s.crc", filename);
    if (rename(filename, new_filename) == -1) {
        err(ERR_FILE, "Cannot rename %s to %s", filename, new_filename);
    }

    // Open it
    FILE *file = fopen(new_filename, "r+");
    if (file == NULL) {
        err(ERR_IO, "Cannot read/write from %s", new_filename);
    }

    // Append (order of poly) zeros to the end of the file
    if (fseek(file, 0L, SEEK_END) == -1) {
        err(ERR_IO, "Cannot seek to the end of %s", new_filename);
    }
    fputc(0, file);
    fputc(0, file);

    // Determine the size of the file
    long file_size = ftell(file);
    if (file_size == 0) {
        err(ERR_IO, "Cannot execute tell on %s", new_filename);
    }

    // Calculate the CRC remainder
    unsigned short crc_rem = calc_crc_rem(file, file_size);

    // Append the checksum to the end of the original data
    if (fseek(file, -CHECKSUM_BYTE_CNT, SEEK_END) == -1) {
        err(ERR_IO, "Cannot seek back in %s", new_filename);
    }
    fputc(crc_rem >> 8, file);
    fputc(crc_rem & 0x00ff, file);

    // Close the file
    if (fclose(file) == EOF) {
        err(ERR_IO, "Error closing %s", new_filename);
    }
}

void validate(char *infilename)
{
    // Determine the size of the file to be validated
    struct stat fileinfo;
    if (stat(infilename, &fileinfo) == -1) {
        err(ERR_FILE, "Cannot execute stat on %s", infilename);
    }
    long filesize = (long) fileinfo.st_size;

    // Open the file to be validated
    FILE *infile = fopen(infilename, "r");
    if (infile == NULL) {
        err(ERR_IO, "Cannot read from %s", infilename);
    }

    // Check whether the CRC remainder is 0
    if (calc_crc_rem(infile, filesize) == 0) {
        warnx("Verification successful.");

        // Determine the name of the original file
        char outfilename[strlen(infilename) + 1];
        strcpy(outfilename, infilename);
        char *crc_ext = strrchr(outfilename, '.');
        if (strcmp(crc_ext, ".crc") != 0) {
            errx(ERR_ARG, "The file probably wasn't a crc-ed file!");
        }
        *crc_ext = '\0';

        // Open it if it is not there already
        FILE *outfile = fopen(outfilename, "wx");
        if (outfile == NULL) {
            if (errno == EEXIST) {
                errx(ERR_FILE, "%s exists. Won't write to it.", outfilename);
            }
            err(ERR_IO, "Cannot write to from %s", outfilename);
        }

        // Go to the beginning of the input file
        if (fseek(infile, 0L, SEEK_SET) == -1) {
            err(ERR_IO, "Cannot go back to the start of %s", infilename);
        }

        // Copy over as many complete blocks as possible
        char buffer[BUFSIZE];
        long portions_cnt = (filesize - CHECKSUM_BYTE_CNT) / BUFSIZE;
        for (int i = 0; i < portions_cnt; ++i) {
            if (fread(buffer, 1, BUFSIZE, infile) != BUFSIZE) {
                err(ERR_IO, "Error reading from %s", infilename);
            }
            if (fwrite(buffer, 1, BUFSIZE, outfile) != BUFSIZE) {
                err(ERR_IO, "Error writing to %s", outfilename);
            }
        }

        // Copy over the remainder, stopping before the checksum
        int yet_to_read_cnt
            = filesize - portions_cnt * BUFSIZE - CHECKSUM_BYTE_CNT;
        if (fread(buffer, 1, yet_to_read_cnt, infile) != yet_to_read_cnt) {
            err(ERR_IO, "Error reading from %s", infilename);
        }
        if (fwrite(buffer, 1, yet_to_read_cnt, outfile) != yet_to_read_cnt) {
            err(ERR_IO, "Error writing to %s", outfilename);
        }

        // Close the files
        if (fclose(infile) == EOF) {
            err(ERR_IO, "Error closing %s", infilename);
        }
        if (fclose(outfile) == EOF) {
            err(ERR_IO, "Error closing %s", outfilename);
        }

        // Remove the file with the checksum
        if (unlink(infilename) == -1) {
            err(ERR_FILE, "Cannot remove %s", infilename);
        }
    }
    else {
        errx(WRONG_CHECKSUM, "Verification failed");
    }
}

unsigned short calc_crc_rem(FILE *file, long filesize)
{
    long bits_cnt          = filesize * 8;
    unsigned short div_reg = 0;

    // Go to the beginning of the input file
    if (fseek(file, 0L, SEEK_SET) == -1) {
        err(ERR_IO, "Cannot go back to the start of file");
    }

    // Shift the whole input file through the division register
    for (int i = 0; i < bits_cnt; ++i) {
        // Record the current highest bit in the register
        int highest_bit = div_reg & 0x8000;

        // Shift in the next bit from the file
        div_reg <<= 1;
        div_reg  |= next_bit_from(file);

        // If the former highest bit was 1, xor the register with the POLY
        if (highest_bit == 0x8000) {
            div_reg ^= POLY;
        }
    }

    return div_reg;
}

int next_bit_from(FILE *file)
{
    static int cur_byte = 0;
    static int cur_offs = 7;

    // Let the offset point to the next bit to be returned
    ++cur_offs;

    // If we have exhausted the last byte read from the file
    if (cur_offs == 8) {
        // Get a fresh byte
        cur_byte = fgetc(file);

        // Reset the pointer to the next bit to be returned
        cur_offs = 0;
    }

    // Return the bit at offset cur_offs in cur_byte
    return (cur_byte >> (7 - cur_offs)) & 1;
}
