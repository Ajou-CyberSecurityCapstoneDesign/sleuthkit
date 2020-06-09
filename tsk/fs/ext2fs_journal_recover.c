#include "tsk/tsk_tools_i.h"
#include "tsk/fs/tsk_fs_i.h"
#include "tsk/fs/tsk_ext2fs.h"
#include "tsk/base/crc.h"
#include <stddef.h>
#include <locale.h>

static TSK_TCHAR *progname;
/* Everything in the journal is in big endian */
#define big_tsk_getu32(x)	\
	(uint32_t)((((uint8_t *)x)[3] <<  0) + \
	(((uint8_t *)x)[2] <<  8) + \
	(((uint8_t *)x)[1] << 16) + \
	(((uint8_t *)x)[0] << 24) )

//grp에 따른 inode table 시작 위치 반환
uint32_t ext2fs_get_blk(TSK_FS_INFO *fs,uint32_t recover_grp){
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;

    unsigned int j;
    int ibpg;
    int gd_size;
    const char *tmptypename;
    
    uint32_t recover_blk;

    switch (fs->ftype) {
    case TSK_FS_TYPE_EXT3:
        tmptypename = "Ext3";
        gd_size = sizeof(ext2fs_gd);
        break;
    case TSK_FS_TYPE_EXT4:
        tmptypename = "Ext4";
        if (EXT2FS_HAS_INCOMPAT_FEATURE(fs, ext2fs->fs,
                EXT2FS_FEATURE_INCOMPAT_64BIT))
            gd_size = sizeof(ext4fs_gd);
        else
            gd_size = sizeof(ext2fs_gd);
        break;
    default:
        tmptypename = "Ext2";
        gd_size = sizeof(ext2fs_gd);
    }
     // number of blocks the inodes consume 
    ibpg =(tsk_getu32(fs->endian,ext2fs->fs->s_inodes_per_group) * ext2fs->inode_size + fs->block_size -1) / fs->block_size;

    //해당 그룹의 inode table 위치 찾기
    for (j = 0;j<ext2fs->groups_count;j++){
        TSK_DADDR_T cg_base;
        TSK_INUM_T inum;
        tsk_take_lock(&ext2fs->lock);

        inum =
            fs->first_inum + tsk_gets32(fs->endian,ext2fs->fs->s_inodes_per_group) * j;
        if (tsk_getu32(fs->endian, ext2fs->fs->s_feature_incompat) & EXT2FS_FEATURE_INCOMPAT_64BIT) {
            cg_base = ext4_cgbase_lcl(fs, ext2fs->fs, j);
        
            if (ext2fs_group_load(ext2fs, j)) {
                tsk_release_lock(&ext2fs->lock);
                return 0;
            }
            if(j==recover_grp){
                if (ext2fs->ext4_grp_buf != NULL) {
                    // The block bitmap is a full block 
                    recover_blk = ext4_getu64(fs->endian,ext2fs->ext4_grp_buf->bg_inode_table_hi,ext2fs->ext4_grp_buf->bg_inode_table_lo);
                    ext4_fsstat_datablock_helper(fs, stdout, j, cg_base, gd_size);
                    tsk_release_lock(&ext2fs->lock);
                    return recover_blk;
                }
                else{ 
                    recover_blk = ext4_getu64(fs->endian,ext2fs->ext4_grp_buf->bg_inode_table_hi,ext2fs->ext4_grp_buf->bg_inode_table_lo);
                    tsk_release_lock(&ext2fs->lock);
                    return recover_blk;
                }
                
            }
        }
        else{
            tsk_release_lock(&ext2fs->lock);
            continue;
        }
    }
    if(j==ext2fs->groups_count){
        return 0;
    }
    //tsk_release_lock(&ext2fs->lock);
    return -1;
}


//삭제된 inode table 저널 영역에서 확인하여 반환해주는 함수

int ext4_jrecover(TSK_FS_INFO *fs, TSK_FS_META * fs_meta, TSK_INUM_T back_inum){

    ext2fs_inode *recover_meta ;
    TSK_INUM_T inum;
    inum = fs->journ_inum;

    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
     /*inum 번호로 몇번 블록의 몇번째 inode 인지 계산*/

    uint32_t recover_grp;
    uint32_t recover_seq;
    uint32_t recover_blk;
    recover_grp = (back_inum-1) / tsk_getu32(fs->endian,ext2fs->fs->s_inodes_per_group);
    recover_seq = ((back_inum-1) % tsk_getu32(fs->endian, ext2fs->fs->s_inodes_per_group)) * ext2fs->inode_size;
    if((recover_blk=ext2fs_get_blk(fs,recover_grp))<0){
        tsk_error_print(stderr);
        ext2fs_close(ext2fs);
        tsk_fs_file_close(fs);
        exit(1);
    }
    
    if (fs->jopen(fs, inum)) {
        tsk_error_print(stderr);
        tsk_fs_file_close(fs);
        ext2fs_close(ext2fs);
        exit(1);
    }

     if ((recover_meta = ext2fs_journal_get_meta(fs, 0, 0, NULL, recover_blk, recover_seq))==NULL) {
        tsk_error_print(stderr);
        tsk_fs_file_close(fs);
        ext2fs_close(ext2fs);
        exit(1);
    }
    
    if(recover_meta==NULL || tsk_getu32(fs->endian,recover_meta->i_ctime)==0){ 
        free(recover_meta);
        return 1;
    }
    else{//메타 복구 시, 덮어쓰기
        if(ext2fs_dinode_copy(ext2fs, fs_meta, back_inum,recover_meta)){
            free(recover_meta);
            exit(1);
        }
        free(recover_meta);
        return 0;
    }
}
