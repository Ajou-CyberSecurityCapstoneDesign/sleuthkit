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
uint64_t ext2fs_get_blk(TSK_FS_INFO *fs,uint32_t recover_grp){
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;

    unsigned int j;
   // int ibpg;
  //  int gd_size;
    const char *tmptypename;
    
     /* number of blocks the inodes consume */
    int ibpg;
    ibpg =
        (tsk_getu32(fs->endian, ext2fs->fs->s_inodes_per_group) * ext2fs->inode_size + fs->block_size - 1) / fs->block_size;

    uint64_t recover_blk;
    //uint64_t recover_blk_2;

    //해당 그룹의 inode table 위치 찾기
    for (j = 0;j<ext2fs->groups_count;j++){
        TSK_DADDR_T cg_base;
        TSK_INUM_T inum;
        tsk_take_lock(&ext2fs->lock);

     
        if (ext2fs_group_load(ext2fs, j)) {
                tsk_release_lock(&ext2fs->lock);
                return -1;
        }

        inum =
            fs->first_inum + tsk_gets32(fs->endian,ext2fs->fs->s_inodes_per_group) * j;
   
    
        if(j==recover_grp){
             if(ext2fs->ext4_grp_buf != NULL){
                // The block bitmap is a full block 
                recover_blk = ext4_getu64(fs->endian,ext2fs->ext4_grp_buf->bg_inode_table_hi,ext2fs->ext4_grp_buf->bg_inode_table_lo);
                tsk_release_lock(&ext2fs->lock);
                return recover_blk;    
             }
             else{
                recover_blk = tsk_getu32(fs->endian, ext2fs->grp_buf->bg_inode_table);
                tsk_release_lock(&ext2fs->lock);
                return recover_blk; 
             } 
        }
        else{
            tsk_release_lock(&ext2fs->lock);
        }
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
    EXT2FS_JINFO *jinfo = ext2fs->jinfo;

     /*inum 번호로 몇번 블록의 몇번째 inode 인지 계산*/
    uint32_t recover_grp;
    uint32_t recover_seq;
    uint64_t recover_blk;
    uint32_t recover_tmp;
    recover_grp = (back_inum-1) / tsk_getu32(fs->endian,ext2fs->fs->s_inodes_per_group);
    recover_seq = ((back_inum-1) % tsk_getu32(fs->endian, ext2fs->fs->s_inodes_per_group));
    recover_tmp = fs->block_size/tsk_getu32(fs->endian,ext2fs->fs->s_inode_size); //블록 내 inode size 개수
    
    if((recover_blk=ext2fs_get_blk(fs,recover_grp))<0){
        tsk_error_print(stderr);
        ext2fs_close(ext2fs);
        tsk_fs_file_close(fs);
        return 1;
    }
    recover_blk += recover_seq / recover_tmp;
    recover_seq = (recover_seq % recover_tmp)*ext2fs->inode_size;

    if (fs->jopen(fs, inum)) {
        ext2fs_close(ext2fs);
        tsk_fs_file_close(fs);
        return 1;
    }

    if ((recover_meta = ext2fs_journal_get_meta(fs, 0, 0, NULL, recover_blk, recover_seq))==NULL){
        return 1;
    }
    else{
        //메타 복구 시, 덮어쓰기
        if(ext2fs_dinode_copy(ext2fs, fs_meta, back_inum,recover_meta))
            return 1;
        return 0;
    }

}
