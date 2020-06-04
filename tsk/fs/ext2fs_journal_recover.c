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
/*
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

    printf("groups count %u\n",ext2fs->groups_count);

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
                }
                else 
                    recover_blk = ext4_getu64(fs->endian,ext2fs->ext4_grp_buf->bg_inode_table_hi,ext2fs->ext4_grp_buf->bg_inode_table_lo);
                return recover_blk;
            }
        }
    }

    return -1;
}
*/

//삭제된 inode table 저널 영역에서 확인하여 반환해주는 함수

ext2fs_inode *ext4_jrecover(TSK_IMG_INFO *img, TSK_FS_INFO *fs, TSK_INUM_T back_inum){

    ext2fs_inode *recover_meta;
    TSK_INUM_T inum;
    inum = fs->journ_inum;

    back_inum = 13;
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;

     /*inum 번호로 몇번 블록의 몇번째 inode 인지 계산*/
    uint32_t recover_grp;
    uint32_t recover_seq;
    uint32_t recover_blk;
    recover_grp = (back_inum-1) / tsk_getu32(fs->endian,ext2fs->fs->s_inodes_per_group);
    recover_seq = ((back_inum-1) % tsk_getu32(fs->endian, ext2fs->fs->s_inodes_per_group)) * ext2fs->inode_size; 
    recover_blk = 1057;
    
    /*
    if((recover_blk=ext2fs_get_blk(fs,recover_grp))<0){
        tsk_error_print(stderr);
        fs->close(fs);
        img->close(img);
        exit(1);
    }
    */

    if (fs->jopen(fs, inum)) {
        tsk_error_print(stderr);
        fs->close(fs);
        img->close(img);
        exit(1);
    }


     if ((recover_meta = ext2fs_journal_get_meta(fs, 0, 0, NULL, recover_blk, recover_seq))==NULL) {
        tsk_error_print(stderr);
        fs->close(fs);
        img->close(img);
        exit(1);
    }

    fs->close(fs);
    img->close(img);
    exit(0);
    //복원 함수로 전달
    return recover_meta;

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
    
    if((recover_meta=ext4_jrecover(img,fs,13))==NULL){
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