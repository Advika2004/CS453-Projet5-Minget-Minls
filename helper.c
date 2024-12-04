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
void get_partition(FILE *disk_image)
{
    // the partition on disk to start looking at is 0
    part_start = 0;

    // if there is no partition specified, just return 
    if(!p_flag)
    {
        return;
    }

    // check that we are on the partition table part of the disk image
    check_partition_table(disk_image);

    //find the partition table 
    if (fseek(disk_image, PARTITION_TABLE_LOCATION + sizeof(struct partition) *
                                                prim_part, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(ERROR);
    }

    // load the info from partition table into the struct
    if (!fread(&part, sizeof(part), 1, disk_image))
    {
        perror("1fread");
        exit(ERROR);
    }

    // since partition is in sectors, convert that to bytes
    part_start = part.lFirst * SECTOR_SIZE;

    //make sure the partition is of the minix type
    check_partition();

    // if there is no sub partition specified function can return
    if (!s_flag)
    {
        return;
    }

    // make sure you are at the sub partition table on disk image 
    check_partition_table(disk_image);

    // find the sub partition in the table
    if (fseek(disk_image, part_start + PARTITION_TABLE_LOCATION +
                     sizeof(struct partition) * sub_part, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(ERROR);
    }

    // load its data into the table 
    if (!fread(&part, sizeof(struct partition), 1, disk_image))
    {
        perror("2fread");
        exit(ERROR);
    }

    // turn the subpartition into bytes as well
    part_start = part.lFirst * SECTOR_SIZE;

    // make sure the sub partition is valid 
    check_partition();
}

//! have to make sure that the partition matches the MINIX magic number 
void check_partition()
{
    if (part.type != MINIX_TYPE) {
        //if the partition is not valid, print it out
        print_partition(part);
        //print error message to stderr
        fprintf(stderr, "Partition at %d is not a minix partition\n",
                part_start);
        exit(ERROR);
    }
}


void check_partition_table(FILE *disk_image)
{
    uint8_t byte510;
    uint8_t byte511;

    // moves file pointer to byte 510 (will have the table signature)
    if (fseek(disk_image, part_start + 510, SEEK_SET) != 0)
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
        fprintf(stderr, "Byte 510 in partition table is not valid: %d\n",
                byte510);
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
void get_superblock(FILE *disk_image)
{
    // if there is no partition specified, we want to just go to first block
    // this would be 1024 after the start of the partition 
    int seek_val = BLOCK_SIZE; 

    // if there is a partition specified, then have to go 1024 bytes after
    // this is where you will find the superblock

    if (p_flag)
    {
        // have to convert the value into bytes
        seek_val += part.lFirst * SECTOR_SIZE;
    }

    // once you calculate where superblock might be, need to search for it
    if (fseek(disk_image, seek_val, SEEK_SET) != 0) {
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
        fprintf(stderr, "Error: filesystem is not a minix filesystem\n");
        exit(ERROR);
    }
}

//! takes the inodes out of the inode table and stores them in a list
void get_inodes(FILE *disk_image)
{
    // have to allocate space to store the inodes 
    // malloc for it because it can change 
    // this creates an array and inodes points to the first one
    inodes = (struct inode *) malloc(sizeof(struct inode) * superblock.ninodes);

    // move the file pointer up until the inode table 
    // this is past the boot block, the  superblock, the bitmap blocks
    if (fseek(disk_image, (part.lFirst * SECTOR_SIZE) + 
                          (2 + superblock.i_blocks + superblock.z_blocks) 
                           * superblock.blocksize, SEEK_SET) != 0) {
        perror("get_inodes() fseek");
        exit(ERROR);
    }

    // read those inodes into memory with inode pointing to the first one
    if (!fread(inodes, sizeof(struct inode), superblock.ninodes, disk_image)) {
        perror("get_inodes() fread");
        exit(ERROR);
    }
}

//! gets the directory entries from the inodes
//! the inode could be storing things in 
//! will return a directory object and get out all the directory info
//? has been changed 

struct directory *get_inodes_in_dir(FILE *disk_image, struct inode *node) {
    // Allocate memory for directory entries
    struct directory *dir = (struct directory *)malloc(node->size);
    struct directory *dir_ptr = dir;  // Pointer to current directory entry

    int bytes_left = node->size;      // must look through the entire inode
    int i = 0;                        // Counter for direct zones
    unsigned int zone;
    int indirect_zone_offset = 0;     // Offset in the indirect zone table

    // Process direct zones first
    while (bytes_left > 0 && i < DIRECT_ZONES) {
        // if the zone is not zero
        if (node->zone[i] != 0) {
            // find what is smaller, whatever is left to read or the whole zone
            int min_size = MIN(bytes_left, zonesize); 
            // put that info into directory struct
            fill_dir(disk_image, dir_ptr, part_start + 
                    node->zone[i] * zonesize, min_size);

            // take that out of how many bytes are left to read
            bytes_left -= min_size;
            // update the pointer by adding how many directories were processed
            dir_ptr += (min_size / sizeof(struct directory));
        }
        i++;
        // move to the next direct zone
    }

    // if out of direct zones and there is still more to read, go to indirect
    if (bytes_left > 0 && node->indirect != 0) {
        for (i = 0; bytes_left > 0 && i < zonesize / 4; i++) {
            // size of zone / size of each entry = number of entries
            // go to the entry in the indirect zone table
            if (fseek(disk_image, node->indirect * zonesize + 
                indirect_zone_offset, SEEK_SET) != 0) {
                perror("fseek");
                exit(ERROR);
            }

            // Read the zone address out from that table
            if (!fread(&zone, sizeof(unsigned int), 1, disk_image)) {
                perror("fread");
                exit(ERROR);
            }

            // move to the next entry
            indirect_zone_offset += 4;

            // if zone address you just got is valid
            if (zone != 0) {
                
                // find what is smaller, the bytes left or the rest of the zone
                int min_size = MIN(bytes_left, zonesize);
                // put that into directory struct
                fill_dir(disk_image, dir_ptr, part_start + zone 
                        * zonesize, min_size);

                bytes_left -= min_size;
                // move the pointer becuase we added more directories
                dir_ptr += (min_size / sizeof(struct directory));
            }
        }
    }

    // return the directory object
    return dir;
}

//! put stuff into the directory object
void fill_dir(FILE *disk_image, struct directory *dir, int location, int size)
{
    // if the location to read from is not zero (not a zero zone)
    if (location != 0)
    {
        // go to that location on disk 
        if (fseek(disk_image, location, SEEK_SET) != 0)
        {
            perror("fseek");
            exit(ERROR);
        }

        // read the data from there and store it into the directory struct
        if (!fread(dir, sizeof(struct directory),
                   size / sizeof(struct directory),  disk_image))
        {
            perror("fread");
            exit(ERROR);
        }
    }
}

//! go through the directory and get the inode info of the file specified
//! put that into the inode struct
//? been changed

struct inode* get_directory_inode(FILE *disk_image, 
                                  struct inode *current_node, 
                                  int path_index) {

    // if we havent looked through the whole path
    if (path_index < src_path_count) {
        // if the type of the current node anded with the directory mask is 0
        if (!(current_node->mode & MASK_DIR)) {
            fprintf(stderr, "Error: %s is not a directory\n", 
                    src_path[path_index]);
            exit(ERROR);
        }
    }

    // if we have gone through the whole path, return the current inode 
    if (path_index >= src_path_count) {
        return current_node;
    }

    // get all the entries in the directory 
    struct directory *dir_entries = get_inodes_in_dir(disk_image, current_node);
    int entry_count = current_node->size / sizeof(struct directory);

    // look for the next part of the path
    int i;
    for (i = 0; i < entry_count; i++) {
        // Compare the current path component with the directory entry name
        if (strcmp(src_path[path_index], (char *)dir_entries[i].name) == 0) {
            // the path matches the name of the file
            struct inode *next_inode = &inodes[dir_entries[i].inode - 1];
            
            // free the entries
            free(dir_entries);

            // keep moving down the path
            return get_directory_inode(disk_image, next_inode, path_index + 1);
        }
    }

    // if the path did not match the file
    fprintf(stderr, "Error: %s not found\n", src_path[path_index]);
    free(dir_entries); // free again 
    exit(ERROR);
}



//! must go through the direct and indirect blocks to read the file data
//! stores the file data into a buffer
void set_file_data(FILE *disk_image, struct inode *node, uint8_t *dst) {
    // the zone address from the indirect zone table can be held here
    unsigned int zone;
    // to iterate over the zones
    int i;
    // stores the offset when reading the zone table
    int indirect_zone_offset = 0;
    // how many bytes still need to be processed
    int bytes_left = node->size;

    // first go through the 7 direct zones where data could be stored
    for(i = 0; i < DIRECT_ZONES && bytes_left > 0; i++) {

        // either will have to read the bytes left or the entire zone
        // whihever is smaller
        int min_size = MIN(bytes_left, zonesize);

        // if the zone is empty, writes zeros to the buffer
        // spec says empty zones are filled with zeros
        // when the zone is empty, and the minimum size is not zero
        // still have to read it out. if min size was zero, dont have to read 
        if (!node->zone[i] && min_size) {
            if (!memset(dst + node->size - bytes_left, 0, min_size)) {
                perror("memset");
                exit(ERROR);
            }
        }
        else {
            // move file descriptor to the direct zone start
            if(fseek(disk_image, part_start + node->zone[i] * zonesize, 
            SEEK_SET) != 0){
                perror("fseek");
                exit(ERROR);
            }

            // make a buffer 
            uint8_t buffer[min_size];
            // read into that buffer
            if (!fread(buffer, 1, min_size, disk_image)) {
                perror("1fread");
                exit(ERROR);
            }

            // copy data from buffer to destination 
            if (!memcpy(dst + node->size - bytes_left, buffer, min_size)) {
                perror("memcpy");
                exit(ERROR);
            }
        }
        // calculate how many bytes still need to be read
        bytes_left -= min_size;
    }

    // if there is still stuff left to read but we run out of direct zones
    //! zonesize/4 can be a macro = the number of entries 
    for(i = 0; i < zonesize/4 && bytes_left > 0; i++) {

        // go to that zone address
        if (fseek(disk_image, node->indirect * zonesize + indirect_zone_offset,
                  SEEK_SET) != 0) {
            perror("fseek");
            exit(ERROR);
        }

        // read what is in there out into the buffer
        if (!fread(&zone, sizeof(unsigned int), 1, disk_image)) {
            perror("2fread");
            exit(ERROR);
        }
        // move to the next entry in the table 
        indirect_zone_offset += 4;

        
        int min_size = MIN(bytes_left, zonesize);


        // check if the indirect zone is empty, then write zeros
        if (!zone && min_size) {
            if (!memset(dst + node->size - bytes_left, 0, min_size)) {
                perror("memset");
                exit(ERROR);
            }
        }

        else {
            // if zone is not empty, read into buffer 
            //! part_start + zone * zonesize could be a macro
            if(fseek(disk_image, part_start + zone * zonesize, SEEK_SET) != 0) {
                perror("fseek");
                exit(ERROR);
            }

            // reads into buffer 
            uint8_t buffer[min_size];
            if (!fread(buffer, 1, min_size, disk_image)) {
                /* Exit on failure */
                if (errno) {
                    perror("fread");
                    exit(ERROR);
                }
                // if the indirect zone is empty, write zeros
                if (!memset(dst + node->size - bytes_left, 0, min_size)) {
                    perror("memset");
                    exit(ERROR);
                }
            }
                //copy it over to destination
            else if(!memcpy(dst + node->size - bytes_left, buffer, min_size)) {
                perror("memcpy");
                exit(ERROR);
            }
        }
        // same thang
        bytes_left -= min_size;
    }
}


//! parsing the path and command line
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

    src_path_count = 0;
    dst_path_count = 0;

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
        src_path = parse_path(s_path, &src_path_count);
    }
    if (imageLoc < argc) {
        d_path = argv[imageLoc++];
        dst_path_string = (char *) malloc(strlen(d_path) + 1);
        strcpy(dst_path_string, d_path);
        dst_path = parse_path(d_path, &dst_path_count);
    }

    /* If no src was provided, default to root */
    if (src_path == NULL) {
        src_path = (char **) malloc(sizeof(char *));
        *src_path = "";
        src_path_count = 0;
    }

    return SUCCESS;
}



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
