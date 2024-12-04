#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include "minfunc.h" //for the structs

/* Macros for File Permissions */
#define FILE_TYPE 0170000
#define SYM_LINK_TYPE 0120000
#define REGULAR_FILE 0100000

#define MASK_DIR  0040000
#define MASK_O_R  0000400
#define MASK_O_W  0000200
#define MASK_O_X  0000100
#define MASK_G_R  0000040
#define MASK_G_W  0000020
#define MASK_G_X  0000010
#define MASK_OT_R 0000004
#define MASK_OT_W 0000002
#define MASK_OT_X 0000001

#define GET_PERM(mode, mask, c) (((mode) & (mask)) == (mask) ? c : '-')


/* Function Declarations */
void print_partition(struct partition part);
void print_super_block(struct superblock sb);
void print_usage(char *argv[]);
void print_inode(struct inode *node);
void print_file(struct inode *node, char *name);
void print_single_file_contents(struct inode *node);
char *get_time(uint32_t time);
char *get_mode(uint16_t mode);

#endif
