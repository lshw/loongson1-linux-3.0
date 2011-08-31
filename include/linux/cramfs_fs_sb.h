#ifndef _CRAMFS_FS_SB
#define _CRAMFS_FS_SB

/*
 * cramfs super-block data in memory
 */

#include <linux/mtd/mtd.h>
#include <mtd/mtd-abi.h> 


struct cramfs_nand_info {		//lxy
    unsigned int erasesize_shift;
    uint32_t *block_map;
    uint32_t size;
};


struct cramfs_sb_info {
			unsigned long magic;
			unsigned long size;
			unsigned long blocks;
			unsigned long files;
			unsigned long flags;

			
			struct cramfs_nand_info cram_nand_inf;		//lxy
			
};

static inline struct cramfs_sb_info *CRAMFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

#endif
