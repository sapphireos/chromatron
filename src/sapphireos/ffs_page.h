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

#ifndef _FFS_PAGE_H
#define _FFS_PAGE_H

#include "ffs_global.h"
#include "ffs_block.h"

#define FFS_PAGE_INDEX_FREE     0xff

typedef struct __attribute__((packed)){
	#ifdef FFS_ALIGN32
    uint8_t data[FFS_PAGE_DATA_SIZE];
    uint8_t len;
    uint8_t padding;
    uint16_t crc;
	#else
    uint8_t len;
    uint8_t data[FFS_PAGE_DATA_SIZE];
    uint16_t crc;
    #endif
} ffs_page_t;


void ffs_page_v_reset( void );

void ffs_page_v_stats_reset( void );
uint32_t ffs_page_u32_get_block_copies( void );
uint32_t ffs_page_u32_get_page_allocs( void );
uint32_t ffs_page_u32_get_cache_hits( void );
uint32_t ffs_page_u32_get_cache_misses( void );

void ffs_page_v_flush( void );
void ffs_page_v_flush_file( ffs_file_t file_id );

void ffs_page_v_init( void );

uint8_t ffs_page_u8_count_files( void );
ffs_file_t ffs_page_i8_file_scan( ffs_file_t file_id );
int32_t ffs_page_i32_scan_file_size( ffs_file_t file_id );

ffs_file_t ffs_page_i8_create_file( void );
int8_t ffs_page_i8_delete_file( ffs_file_t file_id );
int32_t ffs_page_i32_file_size( ffs_file_t file_id );
// ffs_page_t* ffs_page_p_get_cached_page( void );
int8_t ffs_page_i8_read( ffs_file_t file_id, uint16_t page, ffs_page_t **ptr );
int8_t ffs_page_i8_write( ffs_file_t file_id, uint16_t page, uint8_t offset, const void *data, uint8_t len );
block_t ffs_page_i16_replace_block( ffs_file_t file_id, uint8_t file_block );

static inline uint16_t ffs_page_u16_total_pages( void );

static inline uint16_t ffs_page_u16_total_pages( void ){

    return ffs_block_u16_total_blocks() * FFS_PAGES_PER_BLOCK;
}

#endif
