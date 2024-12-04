#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "minfunc.h"
#include "print.h"
#include "helper.h"

int main(int argc, char *argv[])
{
    FILE *image_file_fd;

    if (argc < 2) {
        print_usage(argv);
        return SUCCESS;
    }

    parse_cmd_line(argc, argv);

    if ((image_file_fd = fopen(image_file, "r")) == NULL) {
        perror("open");
        exit(ERROR);
    }

    get_partition(image_file_fd);
    if (v_flag) {
        printf("Partition %d:\n", prim_part);
        print_partition(part);
    }

    get_super_block(image_file_fd);

    get_bitmaps(image_file_fd);

    get_inodes(image_file_fd);

    if (v_flag) {
        print_inode(&inodes[0]);
    }

    if (!src_path_count) {
        exit(ERROR);
    }

    /* Load inode for provided source path */
    struct inode *node = get_directory_inode(image_file_fd, &inodes[0], 0);
    if (!node->size) {
        return SUCCESS;
    }

    /* Error out if file is dir or sym link */
    if((node->mode & MASK_DIR) == MASK_DIR ||
       (node->mode & FILE_TYPE) == SYM_LINK_TYPE) {
        exit(ERROR);
    }

    /* Load in files data into a local buffer */
    uint8_t *dyn_dest = malloc(node->size);
    set_file_data(image_file_fd, node, dyn_dest);

    /* If no destination file was provided, write the local buffer contents
     * to stdout */
    if (!dst_path_count) {
        fwrite(dyn_dest, 1, node->size, stdout);
    }
        /* Otherwise open the destination file and write local buffer contents
         * there */
    else {
        FILE *output;
        if ((output = fopen(dst_path_string, "w")) == NULL) {
            perror("open");
            exit(ERROR);
        }
        fwrite(dyn_dest, 1, node->size, output);
    }

    return SUCCESS;
}




