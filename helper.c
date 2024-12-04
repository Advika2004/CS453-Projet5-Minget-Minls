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
    /* Default offset for primary partition */
    part_start = 0;

    if(!p_flag)
    {
        return;
    }

    /* Ensure partition table is valid */
    check_partition_table(disk_image);

    /* Seek to the primary partition entry in partition table*/
    if (fseek(disk_image, PARTITION_TABLE_LOCATION + sizeof(struct partition) *
                                                prim_part, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(ERROR);
    }

    /* Load partition information from partition table */
    if (!fread(&part, sizeof(part), 1, disk_image))
    {
        perror("1fread");
        exit(ERROR);
    }

    /* Update partition start location offset */
    part_start = part.lFirst * SECTOR_SIZE;

    /* Validate partition */
    check_partition();

    /* If no subpartition was provided we can return here */
    if (!s_flag)
    {
        return;
    }

    /* Validate subpartition table */
    check_partition_table(disk_image);

    /* Seek to desired subpartition entry in the subpartition table */
    if (fseek(disk_image, part_start + PARTITION_TABLE_LOCATION +
                     sizeof(struct partition) * sub_part, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(ERROR);
    }

    /* Load subpartition information from subpartition table */
    if (!fread(&part, sizeof(struct partition), 1, disk_image))
    {
        perror("2fread");
        exit(ERROR);
    }

    /* Update offset of partition to subpartition's start location */
    part_start = part.lFirst * SECTOR_SIZE;

    /* Validate subpartition */
    check_partition();
}

void check_partition()
{
    if (part.type != MINIX_TYPE) {
        print_partition(part);
        fprintf(stderr, "Partition at %d is not a minix partition\n",
                part_start);
        exit(ERROR);
    }
}



void check_partition_table(FILE *disk_image)
{
    uint8_t byte510;
    uint8_t byte511;

    /* Seek to byte 510 in partition table */
    if (fseek(disk_image, part_start + 510, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(ERROR);
    }
    /* Load data at byte 510 */
    if (!fread(&byte510, sizeof(uint8_t), 1, disk_image))
    {
        perror("3fread");
        exit(ERROR);
    }
    /* Validate data at byte 510 */
    if (byte510 != VALID_510) {
        fprintf(stderr, "Byte 510 in partition table is not valid: %d\n",
                byte510);
        exit(ERROR);
    }

    /* Read the data at byte 511 in partition table */
    if (!fread(&byte511, sizeof(uint8_t), 1, disk_image))
    {
        perror("4fread");
        exit(ERROR);
    }
    /* Validate data at byte 511 */
    if (byte511 != VALID_511) {
        fprintf(stderr, "Byte 511 in partition table is not valid\n");
        exit(ERROR);
    }
}

//! superblock stuff

void get_super_block(FILE *disk_image)
{
    /* Set super block's location in filesystem */
    int seek_val = BOOT_SIZE;
    if (p_flag)
    {
        seek_val += part.lFirst * SECTOR_SIZE;
    }

    /* Seek too superblock */
    if (fseek(disk_image, seek_val, SEEK_SET) != 0) {
        perror("fseek");
        exit(ERROR);
    }

    /* Load super block */
    if (!fread(&sb, sizeof(sb), 1, disk_image))
    {
        perror("5fread");
        exit(ERROR);
    }

    /* Update zonesize based on info in superblock */
    zonesize = sb.blocksize << sb.log_zone_size;

    if (v_flag) {
        print_super_block(sb);
    }

    /* Validate */
    validate_superblock();
}

/* Ensure superblock has proper magic number */
void validate_superblock()
{
    if (sb.magic != MAGIC)
    {
        fprintf(stderr, "Error: filesystem is not a minix filesystem\n");
        exit(ERROR);
    }
}


//! bitmap and inode operations

void get_bitmaps(FILE *disk_image)
{
    /* Allocate memory for bitmaps */
    inode_bitmap = (uint8_t *) malloc(sb.blocksize);
    zone_bitmap = (uint8_t *) malloc(sb.blocksize);

    /* Seek past boot block and super block */
    if (fseek(disk_image, 2 * sb.blocksize, SEEK_SET) != 0) {
        perror("fseek");
        exit(ERROR);
    }

    /* Load i-node bitmap */
    if (!fread(inode_bitmap, sb.i_blocks * sb.blocksize, 1, disk_image)) {
        perror("6fread");
        exit(ERROR);
    }
    /* Load zone bitmap */
    if (!fread(zone_bitmap, sb.z_blocks * sb.blocksize, 1, disk_image)) {
        perror("7fread");
        exit(ERROR);
    }
}

/* Load in all inodes on filesystem */
void get_inodes(FILE *disk_image)
{
    /* Allocate memory for local inode list */
    inodes = (struct inode *) malloc(sizeof(struct inode) * sb.ninodes);

    /* Seek to i-node block */
    if (fseek(disk_image, (part.lFirst * SECTOR_SIZE) + (2 + sb.i_blocks +
                                                    sb.z_blocks) 
                                                    * sb.blocksize, 
                                                    SEEK_SET) != 0) {
        perror("get_inodes() fseek");
        exit(ERROR);
    }

    /* Read in inodes into local inode list
     * No need to fseek -- the file pointer is already pointing to
     * inode block at this point */
    if (!fread(inodes, sizeof(struct inode), sb.ninodes, disk_image)) {
        perror("get_inodes() fread");
        exit(ERROR);
    }
}

//! file and directory traversal 

struct directory *get_inodes_in_dir(FILE *disk_image, struct inode *node) {
    /* Allocate enough directory objects for all directories in
     * this inode's data zone */
    struct directory *dir = (struct directory *) malloc(node->size);
    int i = 0;
    int i_max = DIRECT_ZONES;
    int bytes_left = node->size;
    int direct = 1;
    int indirect = 0;
    struct directory *dir_ptr = dir;
    int indirect_zone_offset = 0;
    unsigned int zone;

    while (bytes_left > 0)
    {
        /* if the directories are coming from the direct zone */
        if (direct)
        {
            if (i < i_max)
            {
                int min_size = MIN(bytes_left, zonesize);

                fill_dir(disk_image, dir_ptr, part_start + node->zone[i] 
                         * zonesize,
                         min_size);

                bytes_left -= min_size;
                dir_ptr += (min_size / sizeof(struct directory));
                i++;
            }
            else
            {
                i_max = zonesize / 4;
                i = 0;
                direct = 0;
                indirect = 1;
            }
        }
        else if (indirect)
        {
            if (i < i_max)
            {
                if (fseek(disk_image, node->indirect * zonesize +
                                 indirect_zone_offset, SEEK_SET) != 0)
                {
                    perror("fseek");
                    exit(ERROR);
                }

                if (!fread(&zone, sizeof(unsigned int), 1, disk_image))
                {
                    perror("8fread");
                    exit(ERROR);
                }

                indirect_zone_offset += 4;

                int min_size = MIN(bytes_left, zonesize);

                fill_dir(disk_image, dir_ptr, part_start + zone * zonesize,
                         min_size);

                bytes_left -= min_size;
                dir_ptr += (min_size / sizeof(struct directory));
                i++;
            }
            else
            {
                break;
            }
        }
    }

    return dir;
}

/* Populate a directory object */
void fill_dir(FILE *disk_image, struct directory *dir, int location, int size)
{
    if (location != 0)
    {
        /* Seek to location of the data zone specified by inode */
        if (fseek(disk_image, location, SEEK_SET) != 0)
        {
            perror("fseek");
            exit(ERROR);
        }

        if (!fread(dir, sizeof(struct directory),
                   size / sizeof(struct directory),  disk_image))
        {
            perror("fread");
            exit(ERROR);
        }
    }
}

/* Return inode corresponding to provided source path
 * by starting at root and recursively moving through i-node chain */
struct inode* get_directory_inode(FILE *disk_image, struct inode *node, int arg)
{
    int i;

    if (((node->mode & MASK_DIR) != MASK_DIR) && arg < src_path_count)
    {
        fprintf(stderr, "Error: Incorrect source path\n");
        exit(ERROR);
    }

    if ((node->mode & REGULAR_FILE) == REGULAR_FILE) {
        return node;
    }

    struct directory *dir = get_inodes_in_dir(disk_image, node);

    if (arg < src_path_count)
    {
        for (i = 0; i < node->size / 64; i++)
        {
            if (!strcmp(src_path[arg], (char *) dir[i].name))
            {
                if (arg >= src_path_count)
                {
                    return node;
                }
                arg++;
                struct inode *ret = get_directory_inode(disk_image,
                                            &inodes[dir[i].inode - 1], arg);
                return ret;
            }
        }

        fprintf(stderr, "Error: Incorrect source path\n");
        exit(ERROR);
    }

    return node;
}

//! file data retreival 

void set_file_data(FILE *disk_image, struct inode *node, uint8_t *dst) {
    unsigned int zone;
    int i;
    int indirect_zone_offset = 0;
    int bytes_left = node->size;

    /* Iterate through inodes direct zones */
    for(i = 0; i < DIRECT_ZONES && bytes_left > 0; i++) {

        /* Find size of write */
        int min_size = MIN(bytes_left, zonesize);

        /* If zone is empty */
        if (!node->zone[i] && min_size) {
            if (!memset(dst + node->size - bytes_left, 0, min_size)) {
                perror("memset");
                exit(ERROR);
            }
        }
        else {
            /* Seek to src location in image */
            if(fseek(disk_image, part_start + node->zone[i] * zonesize, 
            SEEK_SET) != 0){
                perror("fseek");
                exit(ERROR);
            }

            /* Load src info into a local buffer */
            uint8_t buffer[min_size];
            if (!fread(buffer, 1, min_size, disk_image)) {
                perror("1fread");
                exit(ERROR);
            }

            /* Write local buffer data to dst */
            if (!memcpy(dst + node->size - bytes_left, buffer, min_size)) {
                perror("memcpy");
                exit(ERROR);
            }
        }
        bytes_left -= min_size;
    }

    /* Iterate through indirect zones */
    for(i = 0; i < zonesize/4 && bytes_left > 0; i++) {
        /* Seek to entry in indirect table */
        if (fseek(disk_image, node->indirect * zonesize + indirect_zone_offset,
                  SEEK_SET) != 0) {
            perror("fseek");
            exit(ERROR);
        }
        /* Load value of entry */
        if (!fread(&zone, sizeof(unsigned int), 1, disk_image)) {
            perror("2fread");
            exit(ERROR);
        }
        indirect_zone_offset += 4;

        int min_size = MIN(bytes_left, zonesize);

        /* If zone is empty */
        if (!zone && min_size) {
            if (!memset(dst + node->size - bytes_left, 0, min_size)) {
                perror("memset");
                exit(ERROR);
            }
        }
        else {
            /* Seek to memory in src */
            if(fseek(disk_image, part_start + zone * zonesize, SEEK_SET) != 0) {
                perror("fseek");
                exit(ERROR);
            }

            /* Attempt to read data from src into local buffer */
            uint8_t buffer[min_size];
            if (!fread(buffer, 1, min_size, disk_image)) {
                /* Exit on failure */
                if (errno) {
                    perror("fread");
                    exit(ERROR);
                }
                /* If src memory was empty, then we can just write 0's  */
                if (!memset(dst + node->size - bytes_left, 0, min_size)) {
                    perror("memset");
                    exit(ERROR);
                }
            }
                /* If write into local buffer was successful, from local
                 * buffer to dst */
            else if(!memcpy(dst + node->size - bytes_left, buffer, min_size)) {
                perror("memcpy");
                exit(ERROR);
            }
        }
        bytes_left -= min_size;
    }

}

//! parsing the path and command line

int parse_cmd_line(int argc, char *argv[])
{
    int isNumber;
    int opt;
    int flagCount;
    int imageLoc;
    char temp[256];
    int tempidx;
    char *s_path;
    char *d_path;

    /* Set all the flags to false to start */
    p_flag = FALSE;
    s_flag = FALSE;
    h_flag = FALSE;
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
            case 'h':
                h_flag = TRUE;
                print_usage(argv);
                exit(ERROR);
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
