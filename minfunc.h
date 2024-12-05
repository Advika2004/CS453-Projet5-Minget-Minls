#ifndef MINFUNC_H
#define MINFUNC_H

#include <stdint.h>


/* Partition Structure */
struct __attribute__ ((__packed__)) partition {
    uint8_t bootind;
    uint8_t start_head;
    uint8_t start_sec;
    uint8_t start_cyl;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sec;
    uint8_t end_cyl;
    uint32_t lFirst;
    uint32_t size;
};

/* Global Variables */
extern struct partition partition;
extern struct superblock sb;
extern struct inode *inodes;

extern unsigned int zonesize;

extern short p_flag, s_flag, h_flag, v_flag;

extern int prim_part, sub_part;
extern uint32_t part_start;

extern char *image_file;
extern char **src_path;
extern char *src_path_string;
extern char **dst_path;
extern char *dst_path_string;

extern int path_arg_count;
extern int destination_path_args;

extern uint8_t *inode_bitmap;
extern uint8_t *zone_bitmap;

#endif
