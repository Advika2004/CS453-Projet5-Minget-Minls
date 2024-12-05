#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "minfunc.h"
#include "print.h"
#include "helper.h"


int main(int argc, char *argv[]) {

    // get dat disk brahhh
    FILE *disk_image;

    // pointer to where the file data will start
    uint8_t *file_data;

    // will hold the node we want to write data from
    struct inode *node;

    // same thing make sure that there is a disk image provided 
    if (argc < 2)
    {
        print_usage(argv);
        return SUCCESS;
    }

    // then parse through it 
    parse_cmd_line(argc, argv);

    // if the disk image does not open return an error 
    if ((disk_image = fopen(image_file, "r")) == NULL) 
    {
        perror("open");
        exit(ERROR);
    }

    // get the partition info 
    partition_info(disk_image);

    // if  there is verbose flag, then print that out
    if (v_flag) 
    {
        printf("Partition %d:\n", prim_part);
        print_partition(partition);
    }

    // get superblock info 
    read_superblock(disk_image);

    // store the inodes 
    read_store_inodes(disk_image);

    // if V print out indoes 
    if (v_flag) 
    {
        print_inode(&inodes[0]);
    }

    // make sure the path was given 
    if (!path_arg_count) 
    {
        fprintf(stderr, "No path specified.\n");
        exit(ERROR);
    }

    // find the node we want from the given path
    node = find_inode_from_path(disk_image, &inodes[0], 0);

    // if there is no node there, say u didnt find it
    if (!node) 
    {
        fprintf(stderr, "File not found\n");
        exit(ERROR);
    }

    // if its not a regular file, exit
    if ((node->mode & MASK_DIR) || (node->mode & FILE_TYPE) == SYM_LINK_TYPE) 
    {
        fprintf(stderr, "Not a regular file\n");
        exit(ERROR);
    }

    // read the file data
    file_data = malloc(node->size);

    if (!file_data) 
    {
        perror("malloc");
        exit(ERROR);
    }

    read_full_file_data(disk_image, node, file_data);

    // Write the file data to the specified destination or stdout
    if (destination_path_args) 
        {
            write_to_output(file_data, node->size, dst_path_string);
        } 
    
    else 
        {
            write_to_output(file_data, node->size, NULL);
        }

    free(file_data); // free my 
    fclose(disk_image); // free em
    return SUCCESS;
}