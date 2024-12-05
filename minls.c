#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "minfunc.h"
#include "print.h"
#include "helper.h"


int main(int argc, char *argv[])
{

    // the disk iamge file
    FILE *disk_image;

    // count for moving through directory entries
    int i;


    // if no disk image given then just print out the usage statement 
    if (argc < 2)
    {
        print_usage(argv);
        return SUCCESS;
    }

    // if there is more to do, parse through the command line
    parse_cmd_line(argc, argv);

    // open the disk image, but if it can't be opened return error
    if ((disk_image = fopen(image_file, "r")) == NULL)
    {
        perror("open");
        exit(ERROR);
    }


    // find the partition table information and read it into struct 
    partition_info(disk_image);

    // if the verbose flag is given here, then print the partition 
    if (v_flag)
    {
        printf("Partition %d:\n", prim_part);
        print_partition(partition);
    }

    // next, read the superblock 
    read_superblock(disk_image);

    // then read all the inodes and store them 
    read_store_inodes(disk_image);

    // if the vflag is on, we want to print everything so print inode info too
    if (v_flag) {
        print_inode(&inodes[0]);
    }

   struct inode *node = find_inode_from_path(disk_image, &inodes[0], 0);
    if (!node) {
        fprintf(stderr, "Path not found");
        exit(ERROR);
    }

    // if the inode found is of type directory, list its stuff
    if ((node->mode & MASK_DIR) == MASK_DIR) {
        print_path();
        printf(":\n");

        // get all the entries out from that inode 
        struct directory *dir = read_entries_from_inode(disk_image, node);
        for (i = 0; i < node->size / sizeof(struct directory); i++) {

            // go through and print evey directory entry
            if (dir[i].inode != 0) {
                print_file(&inodes[dir[i].inode - 1], (char *)dir[i].name);
                printf("\n");
            }
        }
        free(dir); // free my homie
    }

    //  if the path is a regular file, then just print its permissions and size
    else if ((node->mode & REGULAR_FILE) == REGULAR_FILE) {
        print_single_file_contents(node);
        printf(" %s\n", src_path_string);
    }
    // if its neither a file or a directory just exit
    else {
        fprintf(stderr, "Not file or directory");
        exit(ERROR);
    }

    // close the disk
    fclose(disk_image);
    return SUCCESS;
}
