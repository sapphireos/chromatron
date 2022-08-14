/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>
 */

#ifndef _FFS_GLOBAL_H
#define _FFS_GLOBAL_H

#include "system.h"
#include "target.h"
#include "flash_fs_partitions.h"

#define FFS_VERSION                 2

#define FFS_IO_ATTEMPTS             3

#define FLASH_FS_MAX_FILES			( FLASH_FS_MAX_USER_FILES + 2 )
#define FFS_MAX_FILES			    ( FLASH_FS_MAX_FILES - 3 )
// the -3 accounts for the firmware partitions

#define FFS_FILENAME_LEN		    32

#define FFS_FILE_ID_FIRMWARE_0        ( FFS_MAX_FILES )
#define FFS_FILE_ID_FIRMWARE_1        ( FFS_MAX_FILES + 1 )
#define FFS_FILE_ID_FIRMWARE_2        ( FFS_MAX_FILES + 2 )

#define FFS_DIRTY_THRESHOLD         8
#define FFS_WEAR_CHECK_THRESHOLD    64
#define FFS_WEAR_THRESHOLD          1024

typedef int8_t ffs_file_t;

#define FFS_PAGE_DATA_SIZE          64

#ifdef FFS_ALIGN32
#define FFS_DATA_PAGES_PER_BLOCK    48
#else
#define FFS_DATA_PAGES_PER_BLOCK    51
#endif
#define FFS_SPARE_PAGES_PER_BLOCK   8

#define FFS_PAGES_PER_BLOCK         ( FFS_DATA_PAGES_PER_BLOCK + FFS_SPARE_PAGES_PER_BLOCK )
#define FFS_BLOCK_DATA_SIZE         ( FFS_DATA_PAGES_PER_BLOCK * FFS_PAGE_DATA_SIZE )

// function status codes
#define FFS_STATUS_OK   			0
#define FFS_STATUS_BUSY 			-1
#define FFS_STATUS_NO_FREE_SPACE 	-2
#define FFS_STATUS_NO_FREE_FILES 	-3
#define FFS_STATUS_INVALID_FILE 	-4
#define FFS_STATUS_EOF			 	-5
#define FFS_STATUS_ERROR		 	-6
#define FFS_STATUS_PAGE_NOT_ERASED  -7


#endif
