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

#ifndef _FFS_BLOCK_H
#define _FFS_BLOCK_H

#include "target.h"
#include "ffs_global.h"

// flags
#define FFS_FLAG_DIRTY          0x80 // 0 if block is dirty
#define FFS_FLAG_VALID          0x40 // 0 if block is valid

#define FFS_FLAGS_INVALID   0
#define FFS_FLAGS_FREE      0xff

#if FFS_BLOCK_MAX_BLOCKS < 256
typedef uint8_t block_t;
#define FFS_BLOCK_INVALID 0xff
#else
typedef uint16_t block_t;
#define FFS_BLOCK_INVALID 0xffff
#endif

// macro to compute a block address from an FS block number
#define FFS_BLOCK_ADDRESS(block_num) ( ( (uint32_t)block_num * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE ) + (uint32_t)FLASH_FS_FILE_SYSTEM_START )

typedef struct __attribute__((packed, aligned(4))){
    uint8_t file_id;
    uint8_t flags;
    uint8_t block;
    uint8_t sequence;
    uint8_t reserved[4];
} ffs_block_meta_t;

typedef struct __attribute__((packed, aligned(4))){
    uint8_t page_index[FFS_PAGES_PER_BLOCK];
} ffs_block_index_t;

typedef struct __attribute__((packed, aligned(4))){
    ffs_block_meta_t meta;
    ffs_block_index_t index;
} ffs_block_header_t;

#define FFS_META_0(block) ( FFS_BLOCK_ADDRESS(block) )
#define FFS_META_1(block) ( FFS_META_0(block) + sizeof(ffs_block_header_t) )

#define FFS_INDEX_0(block) ( FFS_META_0(block) + sizeof(ffs_block_meta_t) )
#define FFS_INDEX_1(block) ( FFS_INDEX_0(block) + sizeof(ffs_block_header_t) )

typedef struct __attribute__((packed)){
    uint8_t data_pages;
    uint8_t free_pages;
    int8_t phy_last_page;
    int8_t logical_last_page;
    int8_t phy_next_free;
    #ifdef FFS_ALIGN32
    uint8_t padding[3];
    #endif
} ffs_index_info_t;


// public API:
void ffs_block_v_init( void );

int8_t ffs_block_i8_verify_free_space( void );
block_t ffs_block_fb_alloc( void );
block_t ffs_block_fb_get_dirty( void );
void ffs_block_v_add_to_list( block_t *head, block_t block );
void ffs_block_v_replace_in_list( block_t *head, block_t old_block, block_t new_block );
void ffs_block_v_remove_from_list( block_t *head, block_t block );

bool ffs_block_b_is_block_free( block_t block );
bool ffs_block_b_is_block_dirty( block_t block );
bool ffs_block_b_is_in_list( block_t *head, block_t block );
uint8_t ffs_block_u8_list_size( block_t *head );
block_t ffs_block_fb_get_block( block_t *head, uint8_t index );
block_t ffs_block_fb_next( block_t block );

uint8_t ffs_block_u8_read_flags( block_t block );
void ffs_block_v_set_dirty( block_t block );
bool ffs_block_b_is_dirty( const ffs_block_meta_t *meta );
int8_t ffs_block_i8_erase( block_t block );

int8_t ffs_block_i8_read_meta( block_t block, ffs_block_meta_t *meta );
int8_t ffs_block_i8_write_meta( block_t block, const ffs_block_meta_t *meta );
int8_t ffs_block_i8_read_index( block_t block, ffs_block_index_t **index );
int8_t ffs_block_i8_set_index_entry( block_t block, uint8_t logical_index, uint8_t phy_index );
int8_t ffs_block_i8_get_index_info( block_t block, ffs_index_info_t *info );

void ffs_block_v_soft_error( void );
void ffs_block_v_hard_error( void );


// these are extern so the inlining works.
// directly access them at your own risk.
#if FFS_BLOCK_MAX_BLOCKS < 256
extern uint8_t _total_blocks;
extern uint8_t _free_blocks;
extern uint8_t _dirty_blocks;
#else
extern uint16_t _total_blocks;
extern uint16_t _free_blocks;
extern uint16_t _dirty_blocks;
#endif

static inline uint16_t ffs_block_u16_free_blocks( void );
static inline uint16_t ffs_block_u16_dirty_blocks( void );
static inline uint16_t ffs_block_u16_total_blocks( void );

static inline uint16_t ffs_block_u16_free_blocks( void ){

    return _free_blocks;
}

static inline uint16_t ffs_block_u16_dirty_blocks( void ){

    return _dirty_blocks;
}

static inline uint16_t ffs_block_u16_total_blocks( void ){

    #ifdef ENABLE_FFS
    return _total_blocks;
    #else
    return 0;
    #endif
}


#endif
