#include <stdio.h>
#include <sys/types.h>
#include "errors.h"
#include <err.h>

#define IMG_NAME "drive.img"

u_int8_t read_8(FILE *, long);
u_int16_t read_16(FILE *, long);
u_int32_t read_32(FILE *, long);

struct fat_info {
    u_int16_t BPB_BytsPerSec;
    u_int8_t  BPB_SecPerClus;
    u_int16_t BPB_RsvdSecCnt;
    u_int8_t  BPB_NumFATs;
    u_int32_t fat_size;
};

int main(int argc, const char *argv[])
{
    // Check arguments
    if (argc != 2) {
        errx(ARG_ERR, "Directory name must be only command line argument");
    }

    // Open the filesystem image
    FILE *fs_img = fopen(IMG_NAME, "r");
    if (fs_img == NULL) {
        err(INPUT_ERR, "Cannot open %s for reading", IMG_NAME);
    }

    // Read the necessary filesystem information
    struct fat_info fs_info;
    fs_info.BPB_BytsPerSec = read_16(fs_img, 11);
    fs_info.BPB_SecPerClus = read_8( fs_img, 13);
    fs_info.BPB_RsvdSecCnt = read_16(fs_img, 14);
    fs_info.BPB_NumFATs    = read_8( fs_img, 16);

    // Determine the size of one FAT
    u_int16_t fatsz_16 = read_16(fs_img, 22);
    if (fatsz_16 != 0) {
        fs_info.fat_size = (u_int32_t) fatsz_16;
    }
    else {
        fs_info.fat_size = read_32(fs_img, 32);
    }

    printf("%d\n", fs_info.BPB_BytsPerSec);
    printf("%d\n", fs_info.BPB_SecPerClus);
    printf("%d\n", fs_info.BPB_RsvdSecCnt);
    printf("%d\n", fs_info.BPB_NumFATs);
    printf("%d\n", fs_info.fat_size);

    return 0;
}

// Return the 8-bit number at the specified offset in the specified file
u_int8_t read_8(FILE *file, long offset)
{
    // Store the current offset
    long old_offset = ftell(file);
    if (old_offset == -1) {
        err(INPUT_ERR, "Unable to get position in file");
    }

    // Move to the position before the specified offset
    if (fseek(file, offset, SEEK_SET) == -1) {
        err(INPUT_ERR, "Unable to move poisition in file");
    }

    // Read the byte at that position
    int res = fgetc(file);
    if (res == EOF) {
        err(INPUT_ERR, "Error reading");
    }

    // Go back to the old position
    if (fseek(file, old_offset, SEEK_SET) == -1) {
        err(INPUT_ERR, "Cannot return to old position in file");
    }

    return (u_int8_t) res;
}

// Return the 16-bit number at the specified offset in the specified file
u_int16_t read_16(FILE *file, long offset)
{
    // Read the 8-bit number at the specified offset and the number after it
    u_int16_t lower_byte = (u_int16_t) read_8(file, offset);
    u_int16_t upper_byte = (u_int16_t) read_8(file, offset + 1);
        // This is elegant, but rather unefficient.

    // Return their combination into a 16-bit number
    return (upper_byte << 8) + lower_byte;
}

// Return the 32-bit number at the specified offset in the specified file
u_int32_t read_32(FILE *file, long offset)
{
    // Read the 8-bit number at the specified offset and the number after it
    u_int32_t lower_word = (u_int32_t) read_16(file, offset);
    u_int32_t upper_word = (u_int32_t) read_16(file, offset + 1);
        // This is elegant, but rather unefficient.

    // Return their combination into a 16-bit number
    return (upper_word << 16) + lower_word;
}
