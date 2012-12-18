#include <stdio.h>
#include <sys/types.h>
#include "errors.h"
#include <err.h>

#define IMG_NAME "drive.img"
#define PATH_DELIM '/'
#define DIR_ENT_BYTES 32
#define NO_SUCH_ENTRY -1
#define NAME_BYTES 11

u_int8_t read_8(FILE *, long);
u_int16_t read_16(FILE *, long);
u_int32_t read_32(FILE *, long);

struct fat_info {
    u_int16_t BytsPerSec;
    u_int8_t  SecPerClus;
    u_int16_t RsvdSecCnt;
    u_int8_t  NumFATs;
    u_int16_t RootEntCnt;
    u_int32_t fat_size;
    u_int32_t first_root_dir_sec_num;
    u_int32_t first_data_sector;
    u_int16_t dir_ents_per_cluster;
};

struct fat_dir_info {
    char      name[11];
    u_int16_t FstClusLO;
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
    fs_info.BytsPerSec = read_16(fs_img, 11);
    fs_info.SecPerClus = read_8( fs_img, 13);
    fs_info.RsvdSecCnt = read_16(fs_img, 14);
    fs_info.NumFATs    = read_8( fs_img, 16);
    fs_info.RootEntCnt = read_16(fs_img, 17);

    // Determine the size of one FAT
    u_int16_t fatsz_16 = read_16(fs_img, 22);
    if (fatsz_16 != 0) {
        fs_info.fat_size = (u_int32_t) fatsz_16;
    }
    else {
        fs_info.fat_size = read_32(fs_img, 32);
    }

    // Calculate the number of the first sector of the root directory
    int root_dir_sectors = (
                             (fs_info.RootEntCnt * 32)
                             + (fs_info.BytsPerSec - 1)
                           ) / fs_info.BytsPerSec;
    fs_info.first_root_dir_sec_num
        = fs_info.RsvdSecCnt + (fs_info.NumFATs * fs_info.fat_size);

    // Calculate the number of the first data sector
    fs_info.first_data_sector
        = fs_info.first_root_dir_sec_num + root_dir_sectors;

    // Calculate the maximum number of directory entries per cluster
    fs_info.dir_ents_per_cluster
        = fs_info.BytsPerSec * fs_info.SecPerClus / DIR_ENT_BYTES;

    //printf("%d\n", fs_info.BytsPerSec);
    //printf("%d\n", fs_info.SecPerClus);
    //printf("%d\n", fs_info.RsvdSecCnt);
    //printf("%d\n", fs_info.NumFATs);
    //printf("%d\n", fs_info.RootEntCnt);
    //printf("%d\n", fs_info.fat_size);
    //printf("%d\n", fs_info.first_data_sector);
    //printf("%d\n", fs_info.first_root_dir_sec_num);

    // List the specified directory's contents
    ls(fs_info, fs_info.first_root_dir_sec_num , argv[1]);

    return 0;
}

/*
 * List the contents of the specified directory relative to the directory with
 * the specified sector number.
 */
void ls(struct fat_info fs_info, u_int32_t sec_num, char *path)
{
    /*
     * sec_num is the sector number of the "current working directory". A call
     * to this subroutine is the same as "ls path" in that directory.
     * Execution is recursive.
     *
     * Let sec_num point to some directory and path = bla1/bla2/bla3/.
     * This subroutine chops off the first part of path, thus
     *     sub_path = bla2/bla3/
     *     path     = bla1.
     * Then it finds the sector where bla1 (which contains bla2/bla3) starts.
     * That is path_sec_num. With these information it calls ls recursively.
     *
     * Now sec_num, being path_sec_num points to bla1 and path = bla2/bla3/.
     * It chops off the first part again, thus
     *     sub_path = bla3/
     *     path     = bla2.
     * It finds the sector where bla2 starts and stores it in path_sec_num.
     * Recursion, again.
     *
     * Now sec_num points to bla2 and path = bla3/. Chopping yields
     *     sub_path = (empty)
     *     path     = bla3.
     * It finds the sector where bla3 starts. There the recursive call to ls
     * looks for (empty). This is the anchor of recursion and the contents of
     * that directory are printed.
     */

    // List contents of directory at sec_num if only '/' is left as path
    if (strlen(path) == 0) {
        // List contents of directory at sec_num
    }
    // Go one level down the directory hierarchy
    else {
        // Chop the name of the directory at sec_num off the path
        char *sub_path = strchr(path, PATH_DELIM);
        *sub_path = '\0';
        ++sub_path;  // Look above for the effects of this

        // Find the directory entry with that name in the directory at sec_num
        u_int32_t path_sec_num = find_path_sec_num(fs_info, sec_num, path);
            // path contains only the first part of the argument path now

        // List the contents of sub_path relative to the subdirectory
        ls(fs_info, path_sec_num, sub_path);
    }

    return;
}

// Search in the directory at sec_num for the entry with name name and return
// the number of the sector where it starts
u_int32_t find_path_sec_num(struct fat_info fs_info, u_int32_t sec_num,
                            char *name)
{
    // Determine how many entries we have to search at most
    u_int16_t max_entry_cnt;
    if (is_root_dir(fs_info, sec_num)) {
        max_entry_cnt = fs_info.RootEntCnt;
    }
    else {
        max_entry_cnt = fs_info.dir_ents_per_cluster;
    }

    // Calculate where sector sector_nr is in the filesystem
    u_int32_t offset = sec_to_offset(fs_info, sec_num);

    // Search the entries
    for (u_int16_t entry_nr = 0; entry_nr < max_entry_cnt; ++entry_nr) {
        // Calculate the offset of the current entry
        u_int32_t entry_offset = offset + entry_nr * DIR_ENT_BYTES;

        // Obtain the name of this entry
        char entry_name[NAME_BYTES];
        read_dir_entry_name(entry_name, sec_num);

        // Skip the entry if there is nothing in it
        if (*entry_name == 0xe5) {
            continue;
        }

        // Abort the search if there are no more entries
        if (*entry_name == 0x00) {
            return NO_SUCH_ENTRY;
        }

        // Return the sector number the entry points to
        if (namecmp(entry_name, name)) {
            return cluster_to_sec(read_16(entry_offset + 26));
        }
    }

    // We are finished if we were searching the root directory
    if (is_root_dir(fs_info, sec_num)) {
        return NO_SUCH_ENTRY;

    // We are finished too if this one was the last cluster in the chain
    u_int16_t next_cluster_nr = get_next_cluster_nr(fs_info, sec_num);
    if (is_eoc(next_cluster_nr)) {
        return NO_SUCH_ENTRY;
    }
    // If not, continue the search in the next cluster
    else {
        return find_path_sec_num(
                   fs_info,
                   cluster_to_sec(next_cluster_nr),
                   name
               );
    }
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
