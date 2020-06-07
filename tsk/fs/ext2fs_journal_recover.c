#include "tsk/tsk_tools_i.h"
#include "tsk_fs_i.h"
#include "tsk_ext2fs.h"
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


/* usage - explain and terminate */
static void
usage()
{
    TFPRINTF(stderr,
        _TSK_T
        ("usage: %s [-f fstype] [-i imgtype] [-b dev_sector_size] [-o imgoffset] [-vV] image [inode]\n"),
        progname);
    tsk_fprintf(stderr,
        "\t-i imgtype: The format of the image file (use '-i list' for supported types)\n");
    tsk_fprintf(stderr,
        "\t-b dev_sector_size: The size (in bytes) of the device sectors\n");
    tsk_fprintf(stderr,
        "\t-f fstype: File system type (use '-f list' for supported types)\n");
    tsk_fprintf(stderr,
        "\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    tsk_fprintf(stderr, "\t-v: verbose output to stderr\n");
    tsk_fprintf(stderr, "\t-V: print version\n");
    exit(1);
}

/** \internal
    test_root - tests to see if a is power of b
    @param a the number being investigated
    @param b the root
    @return 1 if a is a power of b, otherwise 0
*/

/** \internal
    test_root - tests to see if a is power of b
    @param a the number being investigated
    @param b the root
    @return 1 if a is a power of b, otherwise 0
*/
static uint8_t
test_root(uint32_t a, uint32_t b)
{
    if (a == 0) {
        return b == 0;
    }
    else if (b == 0) {
        return 0;
    }
    else if (a == 1) {
        // anything to power of 0 is 1
        return 1;
    }
    else if (b == 1) {
        return 0;
    }
 
    // keep on multiplying b by itself 
    uint32_t b2;
    for (b2 = b; b2 < a; b2 *= b) {}
 
    // was it an exact match?
    return b2 == a;
}

/** \internal
 * Test if block group has a super block in it.
 *
 * @return 1 if block group has superblock, otherwise 0
*/
static uint32_t
ext2fs_is_super_bg(uint32_t feature_ro_compat, uint32_t group_block)
{
    // if no sparse feature, then it has super block
    if (!(feature_ro_compat & EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER))
        return 1;

    // group 0 always has super block
    if (group_block == 0) 
        return 1;

    // Sparse FS put super blocks in groups that are powers of 3, 5, 7
    if (test_root(group_block, 3) || 
            (test_root(group_block, 5)) ||
            (test_root(group_block, 7))) {
        return 1;
    }

    return 0;
}

static uint8_t
    ext2fs_group_load(EXT2FS_INFO * ext2fs, EXT2_GRPNUM_T grp_num)
{
    TSK_FS_INFO *fs = (TSK_FS_INFO *) ext2fs;
    size_t gd_size = tsk_getu16(fs->endian, ext2fs->fs->s_desc_size);

    /*
    * Sanity check
    */
    if (grp_num >= ext2fs->groups_count) {
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_ARG);
        tsk_error_set_errstr
            ("ext2fs_group_load: invalid cylinder group number: %"
            PRI_EXT2GRP "", grp_num);
        return 1;
    }
    // already loaded
    else if (ext2fs->grp_num == grp_num) {
        return 0;
    }

    // 64-bit version.  
    if (((fs->ftype == TSK_FS_TYPE_EXT4)) && (EXT2FS_HAS_INCOMPAT_FEATURE(fs, ext2fs->fs,
        EXT2FS_FEATURE_INCOMPAT_64BIT)
        && (tsk_getu16(fs->endian, ext2fs->fs->s_desc_size) >= 64))) {
            TSK_OFF_T offs;
            ssize_t cnt;

            if (gd_size < (int)sizeof(ext4fs_gd))
                gd_size = sizeof(ext4fs_gd);

            if (ext2fs->ext4_grp_buf == NULL) {
                if ((ext2fs->ext4_grp_buf = (ext4fs_gd *) tsk_malloc(gd_size)) == NULL) 
                    return 1;
            }
            offs = ext2fs->groups_offset + grp_num * gd_size;

            cnt = tsk_fs_read(&ext2fs->fs_info, offs, (char *) ext2fs->ext4_grp_buf, gd_size);

#ifdef Ext4_DBG
            debug_print_buf((char *) ext2fs->ext4_grp_buf, gd_size);
#endif
            if (cnt != (ssize_t) gd_size) {
                if (cnt >= 0) {
                    tsk_error_reset();
                    tsk_error_set_errno(TSK_ERR_FS_READ);
                }
                tsk_error_set_errstr2("ext2fs_group_load: Group descriptor %"
                    PRI_EXT2GRP " at %" PRIdOFF, grp_num, offs);
                return 1;
            }

            // sanity checks
            if ((ext4_getu64(fs->endian,
                ext2fs->ext4_grp_buf->bg_block_bitmap_hi,
                ext2fs->ext4_grp_buf->bg_block_bitmap_lo) > fs->last_block) ||
                (ext4_getu64(fs->endian,
                ext2fs->ext4_grp_buf->bg_inode_bitmap_hi,
                ext2fs->ext4_grp_buf->bg_inode_bitmap_lo) > fs->last_block) ||
                (ext4_getu64(fs->endian,
                ext2fs->ext4_grp_buf->bg_inode_table_hi,
                ext2fs->ext4_grp_buf->bg_inode_table_lo) > fs->last_block)) {
                    tsk_error_reset();
                    tsk_error_set_errno(TSK_ERR_FS_CORRUPT);
                    tsk_error_set_errstr("extXfs_group_load: Ext4 Group %" PRI_EXT2GRP
                        " descriptor block locations too large at byte offset %"
                        PRIuDADDR, grp_num, offs);
                    return 1;
            }
    }
    else {
        TSK_OFF_T offs;
        ssize_t cnt;
        if (gd_size < (int)sizeof(ext2fs_gd))
            gd_size = sizeof(ext2fs_gd);

        if (ext2fs->grp_buf == NULL) {
            if ((ext2fs->grp_buf = (ext2fs_gd *) tsk_malloc(gd_size)) == NULL) 
                return 1;
        }
        offs = ext2fs->groups_offset + grp_num * gd_size;

        cnt = tsk_fs_read(&ext2fs->fs_info, offs, (char *) ext2fs->grp_buf, gd_size);

        if (cnt != (ssize_t) gd_size) {
            if (cnt >= 0) {
                tsk_error_reset();
                tsk_error_set_errno(TSK_ERR_FS_READ);
            }
            tsk_error_set_errstr2("ext2fs_group_load: Group descriptor %"
                PRI_EXT2GRP " at %" PRIdOFF, grp_num, offs);
            return 1;
        }

        // sanity checks
        if ((tsk_getu32(fs->endian,
            ext2fs->grp_buf->bg_block_bitmap) > fs->last_block) ||
            (tsk_getu32(fs->endian,
            ext2fs->grp_buf->bg_inode_bitmap) > fs->last_block) ||
            (tsk_getu32(fs->endian,
            ext2fs->grp_buf->bg_inode_table) > fs->last_block)) {
                tsk_error_reset();
                tsk_error_set_errno(TSK_ERR_FS_CORRUPT);
                tsk_error_set_errstr("extXfs_group_load: Group %" PRI_EXT2GRP
                    " descriptor block locations too large at byte offset %"
                    PRIuDADDR, grp_num, offs);
                return 1;
        }

        if (tsk_verbose) {
            TSK_FS_INFO *fs = (TSK_FS_INFO *) & ext2fs->fs_info;
           
        }
    }
    ext2fs->grp_num = grp_num;

    return 0;
}

static void
ext4_fsstat_datablock_helper(TSK_FS_INFO * fs, FILE * hFile,
    unsigned int i, TSK_DADDR_T cg_base, int gd_size)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    ext2fs_sb *sb = ext2fs->fs;
    unsigned int gpfbg = (1 << sb->s_log_groups_per_flex);
    unsigned int ibpg, gd_blocks;
    unsigned int num_flex_bg, curr_flex_bg;
    uint64_t last_block;
    ext4fs_gd *ext4_gd = ext2fs->ext4_grp_buf;
    uint64_t db_offset = 0;

    if (ext4_gd == NULL) {
        return;
    }

#ifdef Ext4_DBG
    printf("\nDEBUG 64bit:%d, gd_size %d, combined %d\n",
        EXT2FS_HAS_INCOMPAT_FEATURE(fs, sb, EXT2FS_FEATURE_INCOMPAT_64BIT),
        gd_size >= 64,
        EXT2FS_HAS_INCOMPAT_FEATURE(fs, sb, EXT2FS_FEATURE_INCOMPAT_64BIT)
        && gd_size >= 64);
#endif
    /* number of blocks the inodes consume */
    ibpg =
        (tsk_getu32(fs->endian,
            sb->s_inodes_per_group) * ext2fs->inode_size + fs->block_size -
        1) / fs->block_size;
    /* number of blocks group descriptors consume */
    gd_blocks =
        (unsigned int)((gd_size * ext2fs->groups_count + fs->block_size -
        1) / fs->block_size);
    num_flex_bg = (unsigned int)(ext2fs->groups_count / gpfbg);
    if (ext2fs->groups_count % gpfbg)
        num_flex_bg++;
    curr_flex_bg = i / gpfbg;

    last_block =
        cg_base + tsk_getu32(fs->endian, sb->s_blocks_per_group) - 1;
    if (last_block > fs->last_block) {
        last_block = fs->last_block;
    }

//DEBUG printf("ibpg %d  cur_flex: %d, flex_bgs: %d : %d, %d",ibpg, i/gpfbg, num_flex_bg, ext2fs->groups_count/gpfbg, ext2fs->groups_count%gpfbg);

#ifdef Ext4_DBG
    printf("\nDEBUG: Flex BG PROCESSING cg_base: %" PRIuDADDR
        ", gpfbg: %d, ibpg: %d \n", cg_base, gpfbg, ibpg);
#endif
    /*If this is the 1st bg in a flex bg then it contains the bitmaps and inode tables */
    if (i % gpfbg == 0) {
        if (curr_flex_bg == (num_flex_bg - 1)) {
            unsigned int num_groups = 0;
            unsigned int left_over = 0;

            num_groups = (unsigned int)
                (fs->last_block / tsk_getu32(fs->endian,
                sb->s_blocks_per_group));
            if (num_groups % tsk_getu32(fs->endian,
                    sb->s_blocks_per_group))
                num_groups++;
            left_over = (num_groups % gpfbg);
            
            
        }
       
        db_offset = 0;
        if (ext2fs_is_super_bg(tsk_getu32(fs->endian,
                    sb->s_feature_ro_compat), i)) {
            db_offset = cg_base + (gpfbg * 2)   //To account for the bitmaps
                + (ibpg * gpfbg)        //Combined inode tables
                + tsk_getu16(fs->endian, ext2fs->fs->pad_or_gdt.s_reserved_gdt_blocks) + gd_blocks      //group descriptors
                + 1;            //superblock
        }
        else {
            db_offset = cg_base + (gpfbg * 2)   //To account for the bitmaps
                + (ibpg * gpfbg);       //Combined inode tables
        }
      
    }
    else {
       
        db_offset = 0;
        if (ext2fs_is_super_bg(tsk_getu32(fs->endian,
                    sb->s_feature_ro_compat), i)) {
            db_offset = cg_base + tsk_getu16(fs->endian, ext2fs->fs->pad_or_gdt.s_reserved_gdt_blocks) + gd_blocks      //group descriptors
                + 1;            //superblock
        }
        else {
            db_offset = cg_base;
        }

    }

}




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

    //printf("groups count %u\n",ext2fs->groups_count);

    //해당 그룹의 inode table 위치 찾기
    for (j = 0;j<ext2fs->groups_count;j++){
        TSK_DADDR_T cg_base;
        TSK_INUM_T inum;
        tsk_take_lock(&ext2fs->lock);

        inum =
            fs->first_inum + tsk_gets32(fs->endian,
            ext2fs->fs->s_inodes_per_group) * j;
        
        if (tsk_getu32(fs->endian,
            ext2fs->fs->
            s_feature_incompat) & EXT2FS_FEATURE_INCOMPAT_64BIT) {
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
    }

    return -1;
}


//삭제된 inode table 저널 영역에서 확인하여 반환해주는 함수

TSK_FS_META *ext4_jrecover(TSK_FS_INFO *fs, TSK_FS_META * fs_meta, TSK_INUM_T back_inum){

    ext2fs_inode *recover_meta;
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
    //printf("%u\n",tsk_getu32(fs->endian,recover_meta->i_mtime));

    if(ext2fs_dinode_copy(ext2fs, fs_meta, back_inum,recover_meta)){
        tsk_error_print(stderr);
        tsk_fs_file_close(fs);
        ext2fs_close(ext2fs);
        exit(1);
    }
   
   //printf("meta : %u\n", fs_meta->mtime);

   tsk_release_lock(&ext2fs->lock);
   // tsk_fs_file_close(fs);
   // ext2fs_close(ext2fs);
    //복원 함수로 전달
    return fs_meta;
}
/*

int main(int argc, char**argv1){

    TSK_IMG_TYPE_ENUM imgtype = TSK_IMG_TYPE_DETECT;
    TSK_IMG_INFO *img;

    TSK_OFF_T imgaddr = 0;
    TSK_FS_TYPE_ENUM fstype = TSK_FS_TYPE_DETECT;
    TSK_FS_INFO *fs;

    TSK_INUM_T inum;
    int ch;
    TSK_TCHAR **argv;
    unsigned int ssize = 0;
    TSK_TCHAR *cp;

#ifdef TSK_WIN32
    // On Windows, get the wide arguments (mingw doesn't support wmain)
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) {
        fprintf(stderr, "Error getting wide arguments\n");
        exit(1);
    }
#else
    argv = (TSK_TCHAR **) argv1;
#endif

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = GETOPT(argc, argv, _TSK_T("b:f:i:o:vV"))) > 0) {
        switch (ch) {
        case _TSK_T('?'):
        default:
            TFPRINTF(stderr, _TSK_T("Invalid argument: %s\n"),
                argv[OPTIND]);
            usage();
        case _TSK_T('b'):
            ssize = (unsigned int) TSTRTOUL(OPTARG, &cp, 0);
            if (*cp || *cp == *OPTARG || ssize < 1) {
                TFPRINTF(stderr,
                    _TSK_T
                    ("invalid argument: sector size must be positive: %s\n"),
                    OPTARG);
                usage();
            }
            break;
        case _TSK_T('f'):
            if (TSTRCMP(OPTARG, _TSK_T("list")) == 0) {
                tsk_fs_type_print(stderr);
                exit(1);
            }
            fstype = tsk_fs_type_toid(OPTARG);
            if (fstype == TSK_FS_TYPE_UNSUPP) {
                TFPRINTF(stderr,
                    _TSK_T("Unsupported file system type: %s\n"), OPTARG);
                usage();
            }
            break;
        case _TSK_T('i'):
            if (TSTRCMP(OPTARG, _TSK_T("list")) == 0) {
                tsk_img_type_print(stderr);
                exit(1);
            }
            imgtype = tsk_img_type_toid(OPTARG);
            if (imgtype == TSK_IMG_TYPE_UNSUPP) {
                TFPRINTF(stderr, _TSK_T("Unsupported image type: %s\n"),
                    OPTARG);
                usage();
            }
            break;
        case _TSK_T('o'):
            if ((imgaddr = tsk_parse_offset(OPTARG)) == -1) {
                tsk_error_print(stderr);
                exit(1);
            }
            break;
        case _TSK_T('v'):
            tsk_verbose++;
            break;
        case _TSK_T('V'):
            tsk_version_print(stdout);
            exit(0);
        }
    }

    if (OPTIND >= argc) {
        tsk_fprintf(stderr, "Missing image name and/or address\n");
        usage();
    }


    if (tsk_fs_parse_inum(argv[argc - 1], &inum, NULL, NULL, NULL, NULL)) {
        if ((img =
                tsk_img_open(argc - OPTIND, &argv[OPTIND],
                    imgtype, ssize)) == NULL) {
            tsk_error_print(stderr);
            exit(1);
        }
        if ((imgaddr * img->sector_size) >= img->size) {
            tsk_fprintf(stderr,
                "Sector offset supplied is larger than disk image (maximum: %"
                PRIu64 ")\n", img->size / img->sector_size);
            exit(1);
        }

        if ((fs = tsk_fs_open_img(img, imgaddr * img->sector_size, fstype)) == NULL) {
            tsk_error_print(stderr);
            if (tsk_error_get_errno() == TSK_ERR_FS_UNSUPTYPE)
                tsk_fs_type_print(stderr);
            img->close(img);
            exit(1);
        }

        inum = fs->journ_inum;
    }
    else {
        if ((img =
                tsk_img_open(argc - OPTIND - 1, &argv[OPTIND],
                    imgtype, ssize)) == NULL) {
            tsk_error_print(stderr);
            exit(1);
        }

        if ((fs = tsk_fs_open_img(img, imgaddr * img->sector_size, fstype)) == NULL) {
            tsk_error_print(stderr);
            if (tsk_error_get_errno() == TSK_ERR_FS_UNSUPTYPE)
                tsk_fs_type_print(stderr);
            img->close(img);
            exit(1);
        }
    }

    ext2fs_inode *recover_meta;
    
    if((recover_meta=ext4_jrecover(fs,13))==NULL){
        tsk_error_print(stderr);
        fs->close(fs);
        img->close(img);
        exit(1);
    }

    fs->close(fs);
    img->close(img);
    exit(0);

}

*/
