#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include "helper.h"
#include "print.h"

//! finds the starting location of the partition and subpartition 
//! checks to make sure the parition is valid, then loads its info into globals
//? refactored and working

void partition_info(FILE *disk_image) {
    // start with the partition being 0 as the default
    partition_start = 0;

    // if a partition is specified then process that
    if (p_flag) {
        process_partition(disk_image, prim_part, FALSE);
    }

    // if a subpartition is specified process that too
    if (s_flag) {
        process_partition(disk_image, sub_part, TRUE);
    }
}

void process_partition(FILE *disk_image, int partition_num, 
                        int is_subpartition) {

    // make sure this is of type minix
    check_partition_table(disk_image);

    // find the partition and move the file pointer there
    if (find_partition(disk_image, partition_num, is_subpartition) != 0) {
        perror("fseek");
        exit(ERROR);
    }

    // read partition data into the partition struct
    if (!fread(&partition, sizeof(struct partition), 1, disk_image)) {
        perror("fread");
        exit(ERROR);
    }

    // partition stuff comes in sectors, so convert to bytes
    partition_start = partition.lFirst * SECTOR_SIZE;

    // make sure the partition is of type minix
    check_partition();
}

int find_partition(FILE *disk_image, int partition_num, 
                        int is_subpartition) {

    // the index into the parition table 
    // start location + the size of each partition * how many partitions
    long index = PARTITION_TABLE_LOCATION + sizeof(struct partition) 
                                            * partition_num;

    // if subpartition, adjust the index by adding the main partition size
    if (is_subpartition) {
        index += partition_start;
    }

    // return the location of where that partition starts
    return fseek(disk_image, index, SEEK_SET);
}

//! have to make sure that the partition matches the MINIX magic number
void check_partition()
{
    if (partition.type != FILETYPE_MINIX) {
        //if the partition is not valid, print it out
        print_partition(partition);
        //print error message to stderr
        fprintf(stderr, "Not a minix partition\n");
        exit(ERROR);
    }
}

//! must check if the partition table matches the minix signature
void check_partition_table(FILE *disk_image)
{
    uint8_t byte510;
    uint8_t byte511;

    // moves file pointer to byte 510 (will have the table signature)
    if (fseek(disk_image, partition_start + 510, SEEK_SET) != 0)
    {
        // if it couldn't find that byte return error
        perror("fseek");
        exit(ERROR);
    }

    // if it did work, then read that byte out into byte510
    if (!fread(&byte510, sizeof(uint8_t), 1, disk_image))
    {
        // error if the read didn't work
        perror("3fread");
        exit(ERROR);
    }

    // if byte510 does not equal the signature value
    if (byte510 != PT_510) {
        // print error
        fprintf(stderr, "Byte 510 in partition table is not valid\n");
        exit(ERROR);
    }

    // do the same thing for byte511
    if (!fread(&byte511, sizeof(uint8_t), 1, disk_image))
    {
        perror("4fread");
        exit(ERROR);
    }
    // check to see if what was read from partition table matches the signature
    if (byte511 != PT_511) {
        fprintf(stderr, "Byte 511 in partition table is not valid\n");
        exit(ERROR);
    }
}


//! gets out all the info from the superblock 
void read_superblock(FILE *disk_image)
{
    // if there is no partition specified, we want to just go to first block
    // this would be 1024 after the start of the partition 
    int look_here = BLOCK_SIZE; 

    // if there is a partition specified, then have to go 1024 bytes after
    // this is where you will find the superblock

    if (p_flag)
    {
        // have to convert the value into bytes
        look_here += partition.lFirst * SECTOR_SIZE;
    }

    // once you calculate where superblock might be, need to search for it
    if (fseek(disk_image, look_here, SEEK_SET) != 0) {
        // if can't find it, print error
        perror("fseek");
        exit(ERROR);
    }

    // if you do find it, then read it into the global struct
    if (!fread(&superblock, sizeof(superblock), 1, disk_image))
    {
        //if can't read it, print error
        perror("5fread");
        exit(ERROR);
    }

    // calcualte the zone size (from bit shift in spec)
    zonesize = superblock.blocksize << superblock.log_zone_size;

    // if verbose flag on, print out the superblock info
    if (v_flag) {
        print_super_block(superblock);
    }

    // check that the superblock is of type minix
    check_superblock();
}

//! checks to make sure the superblock is of type Minix
void check_superblock()
{
    // check if the superblock is not of minix type through the magic number
    if (superblock.magic != SUPERBLOCK_MAGIC)
    {
        // then return error
        fprintf(stderr, "Filesystem is not of type Minix\n");
        exit(ERROR);
    }
}

//! takes the inodes out of the inode table and stores them in a list
void read_store_inodes(FILE *disk_image)
{
    // have to allocate space to store the inodes 
    // malloc for it because it can change 
    // this creates an array and inodes points to the first one
    inodes = (struct inode *) malloc(sizeof(struct inode) * superblock.ninodes);

    // move the file pointer up until the inode table 
    // this is past the boot block, the  superblock, the bitmap blocks
    if (fseek(disk_image, (partition.lFirst * SECTOR_SIZE) + 
                          (2 + superblock.i_blocks + superblock.z_blocks) 
                           * superblock.blocksize, SEEK_SET) != 0) {
        fprintf(stderr, "Couldn't find the inode table\n");
        exit(ERROR);
    }

    // read those inodes into memory with inode pointing to the first one
    if (!fread(inodes, sizeof(struct inode), superblock.ninodes, disk_image)) {
        fprintf(stderr, "Couldn't read the inode table\n");
        exit(ERROR);
    }
}

//! gets the directory entries from the inodes
//! the inode could be storing things in 
//! will return a directory object and get out all the directory info
//? has been refactored 

struct directory *read_entries_from_inode(FILE *disk_image, 
                                          struct inode *inode) {
    struct directory *arr_dir = (struct directory *)malloc(inode->size);
    struct directory *curr_dir = arr_dir;
    int bytes_left = inode->size;

    // process direct zones
    process_direct_zones(disk_image, inode, &curr_dir, &bytes_left);

    // process indirect zones
    process_indirect_zones(disk_image, inode, &curr_dir, &bytes_left);

    return arr_dir;
}

void process_direct_zones(FILE *disk_image, struct inode *inode,
                          struct directory **curr_dir, int *bytes_left) {
    int i; // keeps track of how many direct zones have been there
    int zone_size; // how much data is in a zone

    //  if you havent run out of direc zones and there is still data left
    for (i = 0; i < DIRECT_ZONES && *bytes_left > 0; ++i) {

        // if the zone isnt a zero zone
        if (inode->zone[i] != 0) {

            // the size of what you need to read out
            // is either bytes left or the rest of the zone
            zone_size = MIN(*bytes_left, zonesize);

            // move file pointer to the start of that zone
            if (fseek(disk_image, partition_start + inode->zone[i] * zonesize, 
                      SEEK_SET) != 0) {
                fprintf(stderr, "Couldn't find the direct zone\n");
                exit(ERROR);
            }

            // read data from the zone into the directory structure
            if (!fread(*curr_dir, zone_size, 1, disk_image)) {
                fprintf(stderr, "Couldn't read the direct zone\n");
                exit(ERROR);
            }

            // move array pointer since we added directories
            // update how much is left to read
            *bytes_left -= zone_size;
            *curr_dir += zone_size / sizeof(struct directory);
        }
    }
}

void process_indirect_zones(FILE *disk_image, struct inode *inode,
                            struct directory **curr_dir, int *bytes_left) {
    unsigned int zone_address;       // zone address read - indirect zone table
    int indirect_zone_offset = 0;   // index into the indirect zone table
    int zone_size;                  // size of data to read from a zone
    int i;                          // how many zones

    // as long as the indirect zone is not a zero zone
    if (inode->indirect != 0) {

        // zonesize / 4 gives you how many zones there are
        for (i = 0; i < zonesize / IZT_ENTRY_SIZE && *bytes_left > 0; 
             ++i) {
                
            // for every indirect zone, go to its place in the table
            if (fseek(disk_image, inode->indirect * zonesize + 
                      indirect_zone_offset, SEEK_SET) != 0) {
                fprintf(stderr, "Couldn't find the indirect zone address\n");
                exit(ERROR);
            }

            // read the zone address out from the table
            if (!fread(&zone_address, sizeof(unsigned int), 1, disk_image)) {
                fprintf(stderr, "Couldn't read the indirect zone address\n");
                exit(ERROR);
            }

            // move to the next entry in the table
            indirect_zone_offset += IZT_ENTRY_SIZE;

            // ff the zone address is valid, read it
            if (zone_address != 0) {

                // how much to read is whatever is smaller
                // the zone size or how much is left
                zone_size = MIN(*bytes_left, zonesize);

                // move to the start of the zone
            if (fseek(disk_image, partition_start + zone_address * zonesize, 
                          SEEK_SET) != 0) {
                    fprintf(stderr, "Couldn't find the indirect zone data\n");
                    exit(ERROR);
                }

                // read out the zone data (the directory entries)
                if (!fread(*curr_dir, zone_size, 1, disk_image)) {
                    fprintf(stderr, "Couldn't read the indirect zone data\n");
                    exit(ERROR);
                }

                // update update update!
                *bytes_left -= zone_size;
                *curr_dir += zone_size / sizeof(struct directory);
            }
        }
    }
}

//! go through the directory and get the inode info of the file specified
//! put that into the inode struct
//? refactored done 

struct inode* find_inode_from_path(FILE *disk_image, 
                                  struct inode *current_node, 
                                  int curr_arg) {

    // if we havent looked through the whole path
    if (curr_arg < path_arg_count) {

        // if the type of the current node anded with the directory mask is 0
        if (!(current_node->mode & MASK_DIR)) {
            fprintf(stderr, "File is not a directory\n");
            exit(ERROR);
        }
    }

    // if we have gone through the whole path, return the current inode 
    if (curr_arg >= path_arg_count) {
        return current_node;
    }

    // get all the entries in the inode 
    struct directory *dir_entries = read_entries_from_inode(disk_image,
                                                         current_node);
    int entry_count = current_node->size / sizeof(struct directory);

    // look for the next part of the path
    int i;

    for (i = 0; i < entry_count; i++) {
    if (dir_entries[i].inode == 0) {
        fprintf(stderr, "skipping deleted entry\n");
        continue; // skip deleted entries
    }

    if (strcmp(src_path[curr_arg], (char *)dir_entries[i].name) == 0) {
        struct inode *next_inode = &inodes[dir_entries[i].inode - 1];
        free(dir_entries);

    
        // Continue traversal for valid entries
        return find_inode_from_path(disk_image, next_inode, curr_arg + 1);
    }
    }


    // if the path did not match the file
    fprintf(stderr, "Path not found\n");
    free(dir_entries); // free again 
    exit(ERROR);
}


//! must go through the direct and indirect blocks to read the file data
//! stores the file data into a buffer
//? refactored 

void read_full_file_data(FILE *disk_image, struct inode *node, uint8_t *dst) {
    
    // how many bytes still need to be processed
    int bytes_left = node->size;

    // read data from direct zones
    read_direct_zone_data(disk_image, node, dst, &bytes_left);

    //if there are still bytes left, read from indirect zones
    if (bytes_left > 0) {
        read_indirect_zone_data(disk_image, node, dst, &bytes_left);
    }
}

void read_direct_zone_data(FILE *disk_image, struct inode *node, 
                           uint8_t *dst, int *bytes_left) {
    
    // to iterate over the zones
    int i;
    int min_size;

    for (i = 0; i < DIRECT_ZONES && *bytes_left > 0; i++) {

        // either will have to read the bytes left or the entire zone
        // whihever is smaller
        min_size = MIN(*bytes_left, zonesize);

        // if the zone is empty, writes zeros to the buffer
        // when the zone is empty, and the minimum size is not zero
        // still have to read it out. if min size was zero, dont have to read
        if (!node->zone[i] && min_size) {
            memset(dst + node->size - *bytes_left, 0, min_size);
        } 
        
        else 
        {
            // read the actual data into the buffer
            read_zone(disk_image, dst, node->size, *bytes_left, 
                      min_size, node->zone[i]);
        }

        //update how much is left to read
        *bytes_left -= min_size;
    }
}

void read_indirect_zone_data(FILE *disk_image, struct inode *node, 
                             uint8_t *dst, int *bytes_left) {
    int i; // to go through the zones
    int min_size; // how much to read out
    int indirect_zone_offset = 0; // index into the indirect table
    unsigned int zone;

    for (i = 0; i < zonesize / IZT_ENTRY_SIZE && *bytes_left > 0; i++) {
        // go to the indirect zone table entry
        if (fseek(disk_image, node->indirect * zonesize + 
                  indirect_zone_offset, SEEK_SET) != 0) {
            perror("fseek");
            exit(ERROR);
        }

        // read the address of the actual zone into the zone variable
        if (!fread(&zone, sizeof(unsigned int), 1, disk_image)) {
            perror("fread");
            exit(ERROR);
        }

        // move to the next entry in the table
        indirect_zone_offset += IZT_ENTRY_SIZE;

        // now you know which table entry to look at
        // this gives you what indirect zone to actually look at
        // now calculate how much is left to read
        min_size = MIN(*bytes_left, zonesize);

        // if the actual zone is a zero zone fill the buffer with zeros
        if (!zone && min_size) 
        {
            memset(dst + node->size - *bytes_left, 0, min_size);
        } 
        
        // read the data into the actual buffer
        else 
        {
         read_zone(disk_image, dst, node->size, *bytes_left, min_size, zone);
        }

        //update update update
        *bytes_left -= min_size;
    }
}

void read_zone(FILE *disk_image, uint8_t *dst, int node_size, 
               int bytes_left, int size, unsigned int zone) {

    // make the buffer to hold the data
    // make it as large as the inode size 
    uint8_t buffer[size];

    // find the start of the zone to read from 
    if (fseek(disk_image, partition_start + zone * zonesize, SEEK_SET) != 0) {
        perror("fseek");
        exit(ERROR);
    }

    // read into the buffer 
    if (!fread(buffer, 1, size, disk_image)) {
        perror("fread");
        exit(ERROR);
    }

    // copy the buffer into the destination (stdout? if not specified) 
    if (!memcpy(dst + node_size - bytes_left, buffer, size)) {
        perror("memcpy");
        exit(ERROR);
    }
}


// //! parsing the path and command line
int parse_cmd_line(int argc, char *argv[])
{
    int isNumber; 
    int opt; // what getopt returns 
    int flagCount; // how many flags are given
    int imageLoc; // where the disk image is in the arguments
    char temp[256];
    int tempidx;
    char *s_path;
    char *d_path;

    /* Set all the flags to false to start */
    p_flag = FALSE;
    s_flag = FALSE;
    v_flag = FALSE;

    /* Primary and sub partitions initialize to 0 and remain 0 if the
     * following arg of '-p' or '-s' is invalid or does not exist */
    prim_part = 0;
    sub_part = 0;

    image_file = NULL;
    src_path = NULL;
    dst_path = NULL;

    path_arg_count = 0;
    destination_path_args = 0;

    /* Set all the specified flags from the cmd line.
     * Also set prim_part and sub_part if '-p' or '-s' is set */
    flagCount = 0;
    while ((opt = getopt(argc, argv, "vp:s:h")) != -1)
    {
        switch (opt)
        {
            case 'p':
                p_flag = TRUE;
                prim_part = atoi(optarg);
                flagCount++;
                break;
            case 's':
                s_flag = TRUE;
                sub_part = atoi(optarg);
                flagCount++;
                break;
            case 'v':
                v_flag = TRUE;
                flagCount++;
                break;
            default:
                print_usage(argv);
                exit(ERROR);
        }
    }

    /* Walk back through the arguments to find where the image path is */
    imageLoc = 1;
    while(flagCount) {
        if (argv[imageLoc][0] == '-') {
            flagCount--;
        }
        imageLoc++;
    }

    /* Check if current argument is a number */
    strcpy(temp, argv[imageLoc]);
    tempidx = 0;
    isNumber = 1;
    while(temp[tempidx]) {
        if(temp[tempidx] < '0' || temp[tempidx] > '9') {
            isNumber = 0;
            break;
        }
        tempidx++;
    }
    if (isNumber) {
        imageLoc++;
    }

    image_file = argv[imageLoc++];

    if (imageLoc < argc) {
        s_path = argv[imageLoc++];
        src_path_string = (char *) malloc(strlen(s_path) + 1);
        strcpy(src_path_string, s_path);
        src_path = parse_path(s_path, &path_arg_count);
    }
    if (imageLoc < argc) {
        d_path = argv[imageLoc++];
        dst_path_string = (char *) malloc(strlen(d_path) + 1);
        strcpy(dst_path_string, d_path);
        dst_path = parse_path(d_path, &destination_path_args);
    }

    /* If no src was provided, default to root */
    if (src_path == NULL) {
        src_path = (char **) malloc(sizeof(char *));
        *src_path = "";
        path_arg_count = 0;
    }

    return SUCCESS;
}


//!  create an array of strings 
//! each index will hold part of the path
char **parse_path(char *string, int *path_count)
{
    char **path_ptr = (char **) malloc(sizeof(char *));
    int count = 0;

    path_ptr[0] = strtok(string, "/");
    count++;
    while (path_ptr[count - 1] != NULL)
    {
        count++;
        *path_count = *path_count + 1;
        if ((path_ptr = (char**) realloc(path_ptr, sizeof(char *) * count))
            == NULL)
        {
            perror("realloc");
            exit(ERROR);
        }
        if ((path_ptr[count - 1] = (char *) malloc(sizeof(strlen(
                path_ptr[count - 1])))) == NULL)
        {
            perror("malloc");
            exit(ERROR);
        }

        path_ptr[count - 1] = strtok(NULL, "/");
    }

    return path_ptr;
}

void write_to_output(uint8_t *data, size_t size, const char *output_path) {
    
    // where to write to
    FILE *output;

    // if output path is given, then write to it
    if (output_path != NULL) 
    {
        output = fopen(output_path, "w");

        // if it can't be opened then error 
        if (!output) 
        {
            perror("open");
            exit(ERROR);
        }
    } 

    else 
    {
        output = stdout;
    }

    // write to there
    fwrite(data, 1, size, output);

    // close it
    if (output_path != NULL) {
        fclose(output);
    }
}
