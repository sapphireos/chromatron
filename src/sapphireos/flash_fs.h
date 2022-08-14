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

#ifndef _FLASH_FS3_H
#define _FLASH_FS3_H

#include "ffs_global.h"


#define FFS_FILE_PAGE_META_0        0
#define FFS_FILE_PAGE_META_1        1
#define FFS_FILE_PAGE_DATA_0        2

typedef struct __attribute__((packed)){
    char filename[FFS_FILENAME_LEN];
    uint8_t reserved[FFS_PAGE_DATA_SIZE - FFS_FILENAME_LEN];
} ffs_file_meta0_t;

typedef struct __attribute__((packed)){
    uint8_t reserved[FFS_PAGE_DATA_SIZE];
} ffs_file_meta1_t;

#define FFS_FILE_META_SIZE ( sizeof(ffs_file_meta0_t) + sizeof(ffs_file_meta1_t) )


void ffs_v_init( void );
void ffs_v_mount( void );
void ffs_v_format( void );

uint8_t ffs_u8_read_board_type( void );
void ffs_v_write_board_type( uint8_t board );

uint32_t ffs_u32_get_file_count( void );
uint32_t ffs_u32_get_dirty_space( void );
uint32_t ffs_u32_get_free_space( void );
uint32_t ffs_u32_get_available_space( void );
uint32_t ffs_u32_get_total_space( void );


int32_t ffs_i32_get_file_size( ffs_file_t file_id );
int8_t ffs_i8_read_filename( ffs_file_t file_id, char *dst, uint8_t max_len );
ffs_file_t ffs_i8_create_file( char filename[] );
int8_t ffs_i8_delete_file( ffs_file_t file_id );
int32_t ffs_i32_read( ffs_file_t file_id, uint32_t position, void *data, uint32_t len );
int32_t ffs_i32_write( ffs_file_t file_id, uint32_t position, const void *data, uint32_t len );



#endif
