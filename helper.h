#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>
#include "minfunc.h" //for structs

//macros
#define SUCCESS 0
#define ERROR 1
#define TRUE 1
#define FALSE 0

#define IZT_ENTRY_SIZE 4 //indirect zone table entry size

#define DIRECT_ZONES 7
#define PARTITION_TABLE_LOCATION 0x1BE
#define SECTOR_SIZE 512
#define BLOCK_SIZE 1024 // the size of a block
#define SUPERBLOCK_MAGIC 19802

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// the type of the partition (needs to be minix type)
#define FILETYPE_MINIX 0x81 
#define PT_510 0x55 // checks for the valid partition table signature 
#define PT_511 0xAA // samesies

/* Structures */
struct __attribute__ ((__packed__)) superblock {
    uint32_t ninodes;         // number of inodes in this filesystem
    uint16_t pad1;            // make things line up properly
    int16_t i_blocks;         // # of blocks used by inode bit map
    int16_t z_blocks;         // # of blocks used by zone bit map
    uint16_t firstdata;       // number of first data zone
    int16_t log_zone_size;    // log2 of blocks per zone
    int16_t pad2;             // make things line up again
    uint32_t max_file;        // maximum file size
    uint32_t zones;           // number of zones on disk
    int16_t magic;            // magic number */
    int16_t pad3;             // make things line up again
    uint16_t blocksize;       // block size in bytes
    uint8_t subversion;       // filesystem sub–version 
};

struct __attribute__ ((__packed__)) inode {
    uint16_t mode;           
    uint16_t links;           
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
};

struct __attribute__ ((__packed__)) directory {
    uint32_t inode;
    unsigned char name[60];
};


struct partition partition;  
struct superblock superblock;  
struct inode *inodes;  

unsigned int zonesize; 

short p_flag;          
short s_flag;           
short h_flag;
short v_flag;

int prim_part;
int sub_part;

// global representing where the minix file system partition starts
uint32_t partition_start; 

char *image_file;
char **src_path;
char *src_path_string;
char **dst_path;
char *dst_path_string;

int path_arg_count;
int destination_path_args;

//functions
int parse_cmd_line(int argc, char *argv[]);
char **parse_path(char *string, int *path_count);

void partition_info(FILE *fd);
void check_partition_table(FILE *disk_image);
void check_partition();

void read_superblock(FILE *fd);
void check_superblock();

void read_store_inodes(FILE *fd);

struct inode *find_inode_from_path(FILE *fd, struct inode *node, int);

struct directory *read_entries_from_inode(FILE *fd, struct inode *node);

void populate_dir(FILE *image, struct directory *dir, int location, int size);

void read_full_file_data(FILE *image, struct inode *node, uint8_t *ptr);


void initialize_flags();

void handle_flag(char opt, char *optarg, char *argv[]);

int determine_image_location(int argc, char *argv[], int flagCount);

void process_paths(int argc, char *argv[], int imageLoc);


int find_partition(FILE *disk_image, int partition_num, 
                        int is_subpartition);
void process_partition(FILE *disk_image, int partition_num, 
                        int is_subpartition);

void process_indirect_zones(FILE *disk_image, struct inode *inode,
                            struct directory **curr_dir, int *bytes_left);

void process_direct_zones(FILE *disk_image, struct inode *inode,
                          struct directory **curr_dir, int *bytes_left);

void read_zone_to_dir(FILE *disk_image, unsigned int zone_number,
                      struct directory **curr_dir, int *bytes_left, 
                      int zone_size);


void read_direct_zone_data(FILE *disk_image, struct inode *node, 
                           uint8_t *dst, int *bytes_left);

void read_indirect_zone_data(FILE *disk_image, struct inode *node, 
                             uint8_t *dst, int *bytes_left);

void read_zone(FILE *disk_image, uint8_t *dst, int node_size, 
               int bytes_left, int size, unsigned int zone);


void write_to_output(uint8_t *data, size_t size, const char *output_path);


#endif
