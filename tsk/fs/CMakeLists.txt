cmake_minimum_required(VERSION 3.10)
project(tsk_recover_t)

set(TSK_BASE ../../tsk/base/crc.c ../../tsk/base/md5c.c ../../tsk/base/mymalloc.c ../../tsk/base/sha1c.c ../../tsk/base/tsk_endian.c ../../tsk/base/tsk_error.c ../../tsk/base/tsk_error_win32.cpp ../../tsk/base/tsk_list.c ../../tsk/base/tsk_lock.c ../../tsk/base/tsk_parse.c ../../tsk/base/tsk_printf.c ../../tsk/base/tsk_stack.c ../../tsk/base/tsk_unicode.c ../../tsk/base/tsk_version.c ../../tsk/base/XGetopt.c )
set(TSK_FS ../../tsk/fs/apfs_compat.cpp ../../tsk/fs/apfs.cpp ../../tsk/fs/apfs_fs.cpp ../../tsk/fs/apfs_open.cpp ../../tsk/fs/dcalc_lib.c ../../tsk/fs/dcat_lib.c ../../tsk/fs/decmpfs.c ../../tsk/fs/dls_lib.c ../../tsk/fs/dstat_lib.c ../../tsk/fs/exfatfs.c ../../tsk/fs/exfatfs_dent.c ../../tsk/fs/exfatfs_meta.c ../../tsk/fs/ext2fs.c ../../tsk/fs/ext2fs_dent.c ../../tsk/fs/ext2fs_journal.c ../../tsk/fs/fatfs.c ../../tsk/fs/fatfs_dent.cpp ../../tsk/fs/fatfs_meta.c ../../tsk/fs/fatfs_utils.c ../../tsk/fs/fatxxfs.c ../../tsk/fs/fatxxfs_dent.c ../../tsk/fs/fatxxfs_meta.c ../../tsk/fs/ffind_lib.c ../../tsk/fs/ffs.c ../../tsk/fs/ffs_dent.c ../../tsk/fs/fls_lib.c ../../tsk/fs/fs_attr.c ../../tsk/fs/fs_attrlist.c ../../tsk/fs/fs_block.c ../../tsk/fs/fs_dir.c ../../tsk/fs/fs_file.c ../../tsk/fs/fs_inode.c ../../tsk/fs/fs_io.c ../../tsk/fs/fs_load.c ../../tsk/fs/fs_name.c ../../tsk/fs/fs_open.c ../../tsk/fs/fs_parse.c ../../tsk/fs/fs_types.c ../../tsk/fs/hfs.c ../../tsk/fs/hfs_dent.c ../../tsk/fs/hfs_journal.c ../../tsk/fs/hfs_unicompare.c ../../tsk/fs/icat_lib.c ../../tsk/fs/ifind_lib.c ../../tsk/fs/ils_lib.c ../../tsk/fs/iso9660.c ../../tsk/fs/iso9660_dent.c ../../tsk/fs/lzvn.c ../../tsk/fs/nofs_misc.c ../../tsk/fs/ntfs.c ../../tsk/fs/ntfs_dent.cpp ../../tsk/fs/rawfs.c ../../tsk/fs/swapfs.c ../../tsk/fs/unix_misc.c ../../tsk/fs/usnjls_lib.c ../../tsk/fs/usn_journal.c ../../tsk/fs/walk_cpp.cpp ../../tsk/fs/yaffs.cpp)
set(TSK_AUTO ../../tsk/auto/sqlite3.h ../../tsk/auto/auto.cpp ../../tsk/auto/auto_db.cpp ../../tsk/auto/case_db.cpp ../../tsk/auto/db_sqlite.cpp ../../tsk/auto/guid.cpp ../../tsk/auto/is_image_supported.cpp ../../tsk/auto/sqlite3.c ../../tsk/auto/tsk_db.cpp)
set(TSK_POOL ../../tsk/pool/apfs_pool_compat.cpp ../../tsk/pool/tsk_pool.h ../../tsk/pool/apfs_pool.cpp ../../tsk/pool/tsk_pool.h ../../tsk/pool/pool_open.cpp ../../tsk/pool/pool_read.cpp ../../tsk/pool/pool_types.cpp)
set(TSK_IMG ../../tsk/img/aff.c ../../tsk/img/img_io.c ../../tsk/img/img_types.c ../../tsk/img/img_open.cpp ../../tsk/img/mult_files.c ../../tsk/img/raw.c ../../tsk/img/vhd.c ../../tsk/img/vmdk.c ../../tsk/img/img_writer.cpp)
set(TSK_VS ../../tsk/vs/bsd.c ../../tsk/vs/dos.c ../../tsk/vs/gpt.c ../../tsk/vs/mac.c ../../tsk/vs/mm_io.c ../../tsk/vs/mm_open.c ../../tsk/vs/mm_part.c ../../tsk/vs/mm_types.c ../../tsk/vs/sun.c)
set(TSK_HASH ../../tsk/hashdb/binsrch_index.cpp ../../tsk/hashdb/encase.c ../../tsk/hashdb/hashkeeper.c ../../tsk/hashdb/hdb_base.c ../../tsk/hashdb/idxonly.c ../../tsk/hashdb/md5sum.c ../../tsk/hashdb/nsrl.c ../../tsk/hashdb/sqlite_hdb.cpp ../../tsk/hashdb/tsk_hashdb.c)
set(TSK ../../tsk/tsk_tools_i.h)

set(SOURCE_FILES ./ext2fs_journal_recover.c ${TSK_HASH} ${TSK_VS} ${TSK_BASE} ${TSK_FS} ${TSK_AUTO} ${TSK_POOL} ${TSK_IMG} ${TSK})
set(INCLUDE_PATH ../../)

include_directories(${INCLUDE_PATH})

add_executable(ext2fs_journal_recover ${SOURCE_FILES})
target_link_libraries(ext2fs_journal_recover pthread dl)


