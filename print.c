#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include "print.h"
#include "helper.h"

void print_usage(char *argv[])
{
    if (!strcmp(argv[0], "./minls"))
    {
        fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile");
        fprintf(stderr, "[path]\n");
    }
    else if (!strcmp(argv[0], "./minget"))
    {
        fprintf(stderr, "usage: minget [ -v ] [ -p part [ -s subpart ] ]");
        fprintf(stderr, " imagefile srcpath [ dstpath ]");
    }
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-p part    --- select partition for filesystem ");
    fprintf(stderr, "(default: none)\n");
    fprintf(stderr, "-s sub     --- select subpartition for filesystem ");
    fprintf(stderr, "(default: none)\n");
    fprintf(stderr, "-h help    --- print usage information and exit\n");
    fprintf(stderr, "-v verbose --- increase verbosity level\n");
}

void print_partition(struct partition part)
{
    fprintf(stderr, "Partition Contents:\n");
    fprintf(stderr, "bootind      0x%x\n", part.bootind);
    fprintf(stderr, "start_head   %d\n", part.start_head);
    fprintf(stderr, "start_sec    %d\n", part.start_sec);
    fprintf(stderr, "start_cyl    %d\n", part.start_cyl);
    fprintf(stderr, "type         0x%x\n", part.type);
    fprintf(stderr, "end_head     %d\n", part.end_head);
    fprintf(stderr, "end_sec      %d\n", part.end_sec);
    fprintf(stderr, "end_cyl      %d\n", part.end_cyl);
    fprintf(stderr, "lFirst       %lu\n", (unsigned long) part.lFirst);
    fprintf(stderr, "size         %d\n", part.size);
}

void print_super_block(struct superblock sb)
{
    fprintf(stderr, "Superblock Contents:\n");
    fprintf(stderr, "Stored Fields:\n");
    fprintf(stderr, "  ninodes        %d\n", sb.ninodes);
    fprintf(stderr, "  i_blocks       %d\n", sb.i_blocks);
    fprintf(stderr, "  z_blocks       %d\n", sb.z_blocks);
    fprintf(stderr, "  firstdata      %d\n", sb.firstdata);
    fprintf(stderr, "  log_zone_size  %d (zone size: %d)\n",
            sb.log_zone_size, zonesize);
    fprintf(stderr, "  max_file       %d\n", sb.max_file);
    fprintf(stderr, "  magic          0x%04x\n", sb.magic);
    fprintf(stderr, "  zones          %d\n", sb.zones);
    fprintf(stderr, "  blocksize      %d\n", sb.blocksize);
    fprintf(stderr, "  subversion     %d\n\n", sb.subversion);
}

void print_inode(struct inode * node)
{
    int i;
    fprintf(stderr, "File inode:\n");
    fprintf(stderr, "  uint16_t mode       0x%04x (%s)\n", node->mode, get_mode(
            node->mode));
    fprintf(stderr, "  uint16_t links      %d\n", node->links);
    fprintf(stderr, "  uint16_t uid        %d\n", node->uid);
    fprintf(stderr, "  uint16_t gid        %d\n", node->gid);
    fprintf(stderr, "  uint16_t size       %d\n", node->size);
    fprintf(stderr, "  uint32_t atime      %d --- %s", node->atime, get_time(
            node->atime));
    fprintf(stderr, "  uint32_t mtime      %d --- %s", node->mtime, get_time(
            node->mtime));
    fprintf(stderr, "  uint32_t ctime      %d --- %s", node->ctime, get_time(
            node->ctime));


    fprintf(stderr, "\nDirect zones:\n");
    for (i = 0; i < DIRECT_ZONES; i++) {
        fprintf(stderr, "%17s%d] = %5d\n", "zone[", i, node->zone[i]);
    }
    fprintf(stderr, "uint32_t %11s %6d\n", "indirect", node->indirect);
    fprintf(stderr, "uint32_t %9s %8d\n", "double", node->two_indirect);
}

void print_file(struct inode *node, char *name) {
    printf("%s%8d %s", get_mode(node->mode), node->size, name);
}

void print_single_file_contents(struct inode *node)
{
    printf("%s%10d ", get_mode(node->mode), node->size);
}


//! help with formatting
char *get_time(uint32_t time)
{
    time_t t = time;
    return ctime(&t);
}

char *get_mode(uint16_t mode)
{
    char* permissions = (char *) malloc(sizeof(char) * 11);
    permissions[0] = GET_PERM(mode, MASK_DIR, 'd');
    permissions[1] = GET_PERM(mode, MASK_O_R, 'r');
    permissions[2] = GET_PERM(mode, MASK_O_W, 'w');
    permissions[3] = GET_PERM(mode, MASK_O_X, 'x');
    permissions[4] = GET_PERM(mode, MASK_G_R, 'r');
    permissions[5] = GET_PERM(mode, MASK_G_W, 'w');
    permissions[6] = GET_PERM(mode, MASK_G_X, 'x');
    permissions[7] = GET_PERM(mode, MASK_OT_R, 'r');
    permissions[8] = GET_PERM(mode, MASK_OT_W, 'w');
    permissions[9] = GET_PERM(mode, MASK_OT_X, 'x');

    permissions[10] = '\0';

    return permissions;
}