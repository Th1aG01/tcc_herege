#pragma once
// Isola simbolos do LittleFS eventualmente presente no core ESP32.
#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR
#define lfs_crc bancada_lfs_crc
#define lfs_dir_close bancada_lfs_dir_close
#define lfs_dir_open bancada_lfs_dir_open
#define lfs_dir_read bancada_lfs_dir_read
#define lfs_dir_rewind bancada_lfs_dir_rewind
#define lfs_dir_seek bancada_lfs_dir_seek
#define lfs_dir_tell bancada_lfs_dir_tell
#define lfs_file_close bancada_lfs_file_close
#define lfs_file_open bancada_lfs_file_open
#define lfs_file_opencfg bancada_lfs_file_opencfg
#define lfs_file_read bancada_lfs_file_read
#define lfs_file_rewind bancada_lfs_file_rewind
#define lfs_file_seek bancada_lfs_file_seek
#define lfs_file_size bancada_lfs_file_size
#define lfs_file_sync bancada_lfs_file_sync
#define lfs_file_tell bancada_lfs_file_tell
#define lfs_file_truncate bancada_lfs_file_truncate
#define lfs_file_write bancada_lfs_file_write
#define lfs_format bancada_lfs_format
#define lfs_fs_gc bancada_lfs_fs_gc
#define lfs_fs_grow bancada_lfs_fs_grow
#define lfs_fs_mkconsistent bancada_lfs_fs_mkconsistent
#define lfs_fs_size bancada_lfs_fs_size
#define lfs_fs_stat bancada_lfs_fs_stat
#define lfs_fs_traverse bancada_lfs_fs_traverse
#define lfs_getattr bancada_lfs_getattr
#define lfs_migrate bancada_lfs_migrate
#define lfs_mkdir bancada_lfs_mkdir
#define lfs_mount bancada_lfs_mount
#define lfs_remove bancada_lfs_remove
#define lfs_removeattr bancada_lfs_removeattr
#define lfs_rename bancada_lfs_rename
#define lfs_setattr bancada_lfs_setattr
#define lfs_stat bancada_lfs_stat
#define lfs_unmount bancada_lfs_unmount
