/* 
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

	
#ifndef _FS_H
#define _FS_H


#include "cpu.h"
#include "system.h"
#include "memory.h"
#include "ffs_global.h"

#define FS_MAX_FILE_NAME_LEN FFS_FILENAME_LEN
//#define FS_MAX_VIRTUAL_FILES // defined in target.h
#define FS_MAX_FILES ( FLASH_FS_MAX_FILES + FS_MAX_VIRTUAL_FILES )

typedef int8_t file_id_t8;

#define FS_FILE_IS_VIRTUAL(id) ( ( id >= FLASH_FS_MAX_FILES ) && ( id < FS_MAX_FILES ) )

typedef uint8_t mode_t8;
#define FS_MODE_READ_ONLY		    0b00000001
#define FS_MODE_WRITE_APPEND		0b00000100
#define FS_MODE_WRITE_OVERWRITE		0b00001000
#define FS_MODE_CREATE_IF_NOT_FOUND	0b00010000
#define FS_MODE_CREATED				0b00100000
#define FS_MODE_VIRTUAL				0b01000000

typedef uint8_t vfile_op_t8;
#define FS_VFILE_OP_OPEN            1
#define FS_VFILE_OP_READ            2
#define FS_VFILE_OP_WRITE           3
#define FS_VFILE_OP_CLOSE           4
#define FS_VFILE_OP_SIZE            5
#define FS_VFILE_OP_DELETE          6

typedef struct __attribute__((packed)){
    int32_t size; 
    char filename[FS_MAX_FILE_NAME_LEN];
    uint8_t flags;
    uint8_t reserved[15];
} fs_file_info_t;
#define FS_INFO_FLAGS_VIRTUAL       0x01


typedef mem_handle_t file_t;

file_t fs_f_open( char filename[], mode_t8 mode );
file_t fs_f_open_P( PGM_P filename, mode_t8 mode );
file_t fs_f_open_id( file_id_t8 file_id, uint8_t mode );
file_t fs_f_create_virtual( PGM_P filename, 
                            uint32_t (*handler)( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ) );

bool fs_b_busy( void );

void fs_v_init( void );

uint32_t fs_u32_get_virtual_file_count( void );
uint32_t fs_u32_get_file_count( void );

int16_t fs_i16_read( file_t file, void *dst, uint16_t len );
int16_t fs_i16_readline( file_t file, void *dst, uint16_t maxlen );
int16_t fs_i16_write( file_t file, const void *src, uint16_t len );
int32_t fs_i32_get_size( file_t file );
int32_t fs_i32_tell( file_t file );
void fs_v_seek( file_t file, uint32_t pos );
void fs_v_delete( file_t file );
file_t fs_f_close( file_t file );

bool fs_b_exists_id( file_id_t8 id );
file_id_t8 fs_i8_get_file_id( char *filename );
file_id_t8 fs_i8_get_file_id_P( PGM_P filename );
int8_t fs_i8_get_filename_id( file_id_t8 id, void *dst, uint16_t buf_size );
int32_t fs_i32_get_size_id( file_id_t8 id );
int8_t fs_i8_delete_id( file_id_t8 id );
int16_t fs_i16_read_id( file_id_t8 id, uint32_t pos, void *dst, uint16_t len );
int16_t fs_i16_write_id( file_id_t8 id, uint32_t pos, const void *src, uint16_t len );


#endif
