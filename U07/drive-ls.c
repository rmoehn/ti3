#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "errors.h"
#include <err.h>

#define IMG_NAME "drive.img"
#define PATH_DELIM '/'
#define DIR_ENT_BYTES 32
#define NO_SUCH_ENTRY -1
#define NAME_BYTES 11
#define NO_MORE_ENTRIES_STRUCT { "", -1 };

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
    FILE      *fs_img;
};

struct fat_dir_info {
    char      name[11];
    u_int16_t FstClusLO;
};

struct dir_entry_iterator_state {
    // Indicates whether we're traversing the root directory or not
    u_int8_t is_root_dir;

    // The maximum number of directory entries in this cluster (or root dir)
    u_int16_t max_entry_cnt;

    // The number of the current cluster (or -1 for root directory)
    u_int16_t cluster_nr;

    // The byte offset of the start of the current cluster (or root directory)
    u_int32_t cluster_offset;

    // The number of the entry examined last in the cluster
    u_int16_t entry_nr;

    // The entry at entry_nr
    struct fat_dir_info cur_entry;
};

void ls(struct fat_info, u_int32_t, char *);
u_int32_t find_path_sec_num(struct fat_info, u_int32_t, char *);
struct dir_entry_iterator_state new_dir_entry_iterator(struct fat_info,
                                                       u_int32_t);
struct fat_dir_info next_dir_entry(struct fat_info,
        struct dir_entry_iterator_state *);

int name_equals(char *, char *);
u_int32_t cluster_to_sec(struct fat_info, u_int16_t);
u_int32_t sec_to_offset(struct fat_info, u_int32_t);
void sprint_filename(char *, char *);
int is_last_entry(struct fat_dir_info);
struct fat_dir_info no_more_entries();
u_int16_t get_next_cluster_nr(struct fat_info, u_int16_t);
int is_eoc(u_int16_t);
struct fat_dir_info read_dir_entry(FILE *, u_int32_t);

u_int8_t read_8(FILE *, long);
u_int16_t read_16(FILE *, long);
u_int32_t read_32(FILE *, long);

int main(int argc, char *argv[])
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
    fs_info.fs_img = fs_img;
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
        // Initialise the iterator for directory entries
        struct dir_entry_iterator_state dit_state
            = new_dir_entry_iterator(fs_info, sec_num);

        // Go through the directory entries
        struct fat_dir_info entry;
        while (! is_last_entry(entry = next_dir_entry(fs_info, &dit_state))) {
            // Print them
            char pretty_name[NAME_BYTES];
            sprint_filename(pretty_name, entry.name);
            puts(pretty_name);
        }
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
    // Initialise the iterator for directory entries
    struct dir_entry_iterator_state dit_state
        = new_dir_entry_iterator(fs_info, sec_num);

    // Go through the directory entries
    struct fat_dir_info entry;
    while (! is_last_entry(entry = next_dir_entry(fs_info, &dit_state))) {
        // Return the indicated sector number if we have found the right entry
        if (name_equals(entry.name, name)) {
            return cluster_to_sec(fs_info, entry.FstClusLO);
        }
    }

    // Return failure if there are no more entries
    return NO_SUCH_ENTRY;
}

// Construct an iterator for traversing a directory. The start of the
// directory is indicated by sec_num, which must be the number of first sector
// of a cluster.
struct dir_entry_iterator_state new_dir_entry_iterator(
        struct fat_info fs_info, u_int32_t sec_num)
{
    struct dir_entry_iterator_state dit_state;

    // Set some values depending on whether we're in the root directory or not
    if (sec_num == fs_info.first_root_dir_sec_num) {
        dit_state.is_root_dir   = 1;
        dit_state.max_entry_cnt = fs_info.RootEntCnt;
        dit_state.cluster_nr    = -1;
    }
    else {
        dit_state.is_root_dir   = 0;
        dit_state.max_entry_cnt = fs_info.dir_ents_per_cluster;
        dit_state.cluster_nr    = (sec_num - fs_info.first_data_sector)
                                  / fs_info.SecPerClus + 2;
    }

    dit_state.cluster_offset = sec_to_offset(fs_info, sec_num);
    dit_state.entry_nr       = -1;
        // See above for descriptions of the struct's fields

    return dit_state;
}

// Look whether there are more entries in the traversed directory.
struct fat_dir_info next_dir_entry(struct fat_info fs_info,
                                   struct dir_entry_iterator_state *dit_state)
{
    // Increment entry number
    ++(dit_state->entry_nr);

    // Check whether we're at the end of a directory
    if (dit_state->entry_nr == dit_state->max_entry_cnt) {
        // We have no more entries if we're at the end of the root directory
        if (dit_state->is_root_dir) {
            return no_more_entries();
        }

        // Otherwise go to the next cluster in the chain if it exists
        u_int16_t next_cluster_nr
            = get_next_cluster_nr(fs_info, dit_state->cluster_nr);
        if (is_eoc(next_cluster_nr)) {
            return no_more_entries();
        }
        else {
            dit_state->cluster_nr = next_cluster_nr;
            dit_state->cluster_offset
                = cluster_to_sec(fs_info, next_cluster_nr);
            dit_state->entry_nr   = -1;
        }
    }

    // Read in the current directory entry
    struct fat_dir_info cur_entry = read_dir_entry(
                                        fs_info.fs_img,
                                        dit_state->cluster_offset
                                        + dit_state->entry_nr * DIR_ENT_BYTES
                                    );

    // Stop if we are at a last directory entry
    if (*(cur_entry.name) == 0x00) {
        --(dit_state->entry_nr);
            // So that we're at the same entry in the next call
        return no_more_entries();
    }

    // Skip the entry if it is empty
    if (*(cur_entry.name) == 0xe5) {
        return next_dir_entry(fs_info, dit_state);
    }

    return cur_entry;
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
