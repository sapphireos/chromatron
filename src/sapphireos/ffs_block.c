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


#include "cpu.h"
#include "crc.h"
#include "system.h"
#include "timers.h"
#include "keyvalue.h"
#include "memory.h"

#include "ffs_page.h"
#include "ffs_block.h"
#include "ffs_global.h"
#include "flash25.h"
#include "flash_fs_partitions.h"

#ifdef ENABLE_FFS

typedef struct{
	block_t next_block;
} block_info_t;

typedef struct{
    block_t block;
    ffs_block_index_t index;
} index_cache_t;

static index_cache_t index_cache;

static mem_handle_t blocks_h;

#if FFS_BLOCK_MAX_BLOCKS < 256
uint8_t _total_blocks;
uint8_t _free_blocks;
uint8_t _dirty_blocks;
#else
uint16_t _total_blocks;
uint16_t _free_blocks;
uint16_t _dirty_blocks;
#endif

static block_t free_list;
static block_t dirty_list;

#ifdef FLASH_FS_TIMING
static block_t free_scan_list;
#endif

static uint8_t flash_fs_soft_io_errors;
static uint8_t flash_fs_hard_io_errors;
static uint32_t flash_fs_block_allocs;

KV_SECTION_META kv_meta_t ffs_block_info_kv[] = {
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &flash_fs_soft_io_errors,        0,  "flash_fs_soft_io_errors" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_block_allocs,          0,  "flash_fs_block_allocs" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &flash_fs_hard_io_errors,        0,  "flash_fs_hard_io_errors" },
};




#ifdef FLASH_FS_TIMING
static uint32_t flash_fs_timing_ffs_block_scan;
static uint32_t flash_fs_timing_ffs_free_scan;

KV_SECTION_META kv_meta_t ffs_block_timing_info_kv[] = {
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_timing_ffs_block_scan,         0,  "flash_fs_timing_ffs_block_scan" },    
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_timing_ffs_free_scan,         0,  "flash_fs_timing_ffs_free_scan" },    
};


PT_THREAD( free_scan_thread( pt_t *pt, void *state ) );

#endif



static inline block_info_t *get_block_ptr( void ) __attribute__((always_inline));

static inline block_info_t *get_block_ptr( void ){

    return (block_info_t *)mem2_vp_get_ptr( blocks_h );
}


void ffs_block_v_init( void ){

    index_cache.block = FFS_BLOCK_INVALID;
    blocks_h = FFS_BLOCK_INVALID;
    _total_blocks = 0;

    // initialize data structures
	free_list = FFS_BLOCK_INVALID;
	dirty_list = FFS_BLOCK_INVALID;

    #ifdef FLASH_FS_TIMING
    free_scan_list = FFS_BLOCK_INVALID;
    #endif

	_free_blocks = 0;
	_dirty_blocks = 0;

    // if( sys_u8_get_mode() == SYS_MODE_SAFE ){

    //     return;
    // }

    // check size of flash device
    uint32_t flash_size = flash25_u32_capacity();
    uint16_t n_blocks = flash_size / FLASH_FS_ERASE_BLOCK_SIZE;

    n_blocks -= FLASH_FS_FILE_SYSTEM_START_BLOCK;

    if( n_blocks > FFS_BLOCK_MAX_BLOCKS ){

        n_blocks = FFS_BLOCK_MAX_BLOCKS;
    }

    _total_blocks = n_blocks;

    // allocate memory
    blocks_h = mem2_h_alloc2( _total_blocks * sizeof(block_info_t), MEM_TYPE_FS_BLOCKS );

    ASSERT( blocks_h >= 0 );

    block_info_t *blocks = get_block_ptr();

	for( uint16_t i = 0; i < _total_blocks; i++ ){

		blocks[i].next_block = FFS_BLOCK_INVALID;
	}

    trace_printf("Block scan...\r\n");

    #ifdef FLASH_FS_TIMING
    uint32_t start_time = tmr_u32_get_system_time_ms();
    #endif

    ffs_block_meta_t meta;

    // scan and verify all block headers
    for( uint16_t i = 0; i < _total_blocks; i++ ){

        sys_v_wdt_reset();

        // read header flags
        uint8_t flags = ffs_block_u8_read_flags( i );

        // check if free
        if( flags == FFS_FLAGS_FREE ){

            // add to free list
            #ifdef FLASH_FS_TIMING
            ffs_block_v_add_to_list( &free_scan_list, i );
            #else
            ffs_block_v_add_to_list( &free_list, i );
            #endif
        }
        // check if marked as dirty
        else if( ~flags & FFS_FLAG_DIRTY ){

            // add to dirty list
            ffs_block_v_add_to_list( &dirty_list, i );
        }
        // check if marked as valid
        else if( flags == (uint8_t)~FFS_FLAG_VALID ){

            // read meta header and check for read errors
            if( ffs_block_i8_read_meta( i, &meta ) < 0 ){

                // read error, mark as dirty
                ffs_block_v_set_dirty( i );

                continue;
            }
        }
        else{ // flags are marked as something else

            // read error, mark as dirty
            ffs_block_v_set_dirty( i );
        }
    }

    #ifdef FLASH_FS_TIMING
    flash_fs_timing_ffs_block_scan = tmr_u32_elapsed_time_ms( start_time );
    #endif

    // verify free space
    ffs_block_i8_verify_free_space();

    #ifdef FLASH_FS_TIMING
    flash_fs_timing_ffs_free_scan = tmr_u32_elapsed_time_ms( flash_fs_timing_ffs_block_scan );
    #endif
}



#ifdef FLASH_FS_TIMING
PT_THREAD( free_scan_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static block_t block;
    block = free_scan_list;

    while( block != FFS_BLOCK_INVALID ){

        TMR_WAIT( pt, 10 );

        bool block_ok = TRUE;

        // check all data in block
        uint8_t data[256] __attribute__((aligned(4)));
        uint32_t addr = FFS_BLOCK_ADDRESS( block );

        for( uint8_t j = 0; j < FLASH_FS_ERASE_BLOCK_SIZE / 256; j++ ){

            sys_v_wdt_reset();

            flash25_v_read( addr, data, sizeof(data) );

            for( uint16_t h = 0; h < sizeof(data); h++ ){

                if( data[h] != 0xff ){

                    block_ok = FALSE;

                    ffs_block_v_remove_from_list( &free_scan_list, block );                        
                    ffs_block_v_set_dirty( block );

                    goto scan_next_block;
                }
            }

            addr += sizeof(data);
        }
        // loop exit if block is OK

        if( block_ok ){   
            
            // move from scan list to free list
            ffs_block_v_remove_from_list( &free_scan_list, block );
            ffs_block_v_add_to_list( &free_list, block );
        }   


scan_next_block:

        block = ffs_block_fb_next( block );
    }


PT_END( pt );
}
#endif


int8_t ffs_block_i8_verify_free_space( void ){

    trace_printf("Verify free space...\r\n");

    uint16_t scan_count;

    #ifdef FLASH_FS_TIMING
    block_t block = free_scan_list;

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        scan_count = 65535; // scans all blocks
    }
    else{

        scan_count = 8; // number of blocks to scan

        thread_t_create( free_scan_thread, PSTR("free_scan"), 0, 0 );
    }
    
    #else
    block_t block = free_list;
    scan_count = 65535; // scans all blocks
    #endif

    bool errors = FALSE;

    while( ( block != FFS_BLOCK_INVALID ) && ( scan_count > 0 ) ){

        scan_count--;

        #ifdef FLASH_FS_TIMING
        bool block_ok = TRUE;
        #endif

        // check all data in block
        uint8_t data[256] __attribute__((aligned(4)));
        uint32_t addr = FFS_BLOCK_ADDRESS( block );

        for( uint8_t j = 0; j < FLASH_FS_ERASE_BLOCK_SIZE / 256; j++ ){

            sys_v_wdt_reset();

            flash25_v_read( addr, data, sizeof(data) );

            for( uint16_t h = 0; h < sizeof(data); h++ ){

                if( data[h] != 0xff ){

                    errors = TRUE;
                    
                    #ifdef FLASH_FS_TIMING
                    block_ok = FALSE;

                    ffs_block_v_remove_from_list( &free_scan_list, block );
                    #else
                    ffs_block_v_remove_from_list( &free_list, block );
                    #endif

                    ffs_block_v_set_dirty( block );

                    goto scan_next_block;
                }
            }

            addr += sizeof(data);
        }
        // loop exit if block is OK

        #ifdef FLASH_FS_TIMING
        if( block_ok ){

            // move from scan list to free list
            ffs_block_v_remove_from_list( &free_scan_list, block );
            ffs_block_v_add_to_list( &free_list, block );
        }   
        #endif


scan_next_block:

        block = ffs_block_fb_next( block );
    }

    if( errors ){

        return FFS_STATUS_ERROR;
    }

    return FFS_STATUS_OK;
}

// allocate a free block.
// note if the caller doesn't use the block, it is effectively removed from the file system until a remount
block_t ffs_block_fb_alloc( void ){

    if( _free_blocks == 0 ){

        return FFS_BLOCK_INVALID;
    }

    flash_fs_block_allocs++;

	block_t block = free_list;

    block_info_t *blocks = get_block_ptr();
	free_list = blocks[block].next_block;

	blocks[block].next_block = FFS_BLOCK_INVALID;

	_free_blocks--;

    // return block
    return block;
}

block_t ffs_block_fb_get_dirty( void ){

    if( _dirty_blocks == 0 ){

        return FFS_BLOCK_INVALID;
    }

	block_t block = dirty_list;

    block_info_t *blocks = get_block_ptr();
	dirty_list = blocks[block].next_block;

	blocks[block].next_block = FFS_BLOCK_INVALID;

	_dirty_blocks--;

    // return block
    return block;
}



void ffs_block_v_add_to_list( block_t *head, block_t block ){

    // ensure block will mark the end of the list
    block_info_t *blocks = get_block_ptr();
    blocks[block].next_block = FFS_BLOCK_INVALID;

	// check if list is empty
	if( *head == FFS_BLOCK_INVALID ){

		*head = block;
	}
    else{ // need to walk to end of list

        block_t cur_block = *head;

        //ASSERT( head < FFS_FILE_SYSTEM_N_BLOCKS ); // this gets checked in the loop
        ASSERT( block < (block_t)_total_blocks );

        while(1){

            ASSERT( cur_block < (block_t)_total_blocks );

            if( blocks[cur_block].next_block == FFS_BLOCK_INVALID ){

                // add to list
                blocks[cur_block].next_block = block;

                break;
            }

            // advance
            cur_block = blocks[cur_block].next_block;
        }
    }

    // check list
    if( head == &dirty_list ){

        // increment count
        _dirty_blocks++;
    }
    else if( head == &free_list ){

        // increment count
        _free_blocks++;
    }
}

void ffs_block_v_replace_in_list( block_t *head, block_t old_block, block_t new_block ){

	ASSERT( old_block < (block_t)_total_blocks );
	ASSERT( new_block < (block_t)_total_blocks );

    block_info_t *blocks = get_block_ptr();

	if( *head == old_block ){

		*head = new_block;
	}
	else{

		block_t prev_block = *head;

		// seek to the block before old_block
		while( blocks[prev_block].next_block != old_block ){

			prev_block = blocks[prev_block].next_block;

			ASSERT( prev_block != FFS_BLOCK_INVALID );
		}

		blocks[prev_block].next_block = new_block;
	}

	blocks[new_block].next_block = blocks[old_block].next_block;
}

void ffs_block_v_remove_from_list( block_t *head, block_t block ){

	ASSERT( block < (block_t)_total_blocks );

    block_info_t *blocks = get_block_ptr();

	if( *head == block ){

		*head = blocks[block].next_block;
	}
	else{

		block_t prev_block = *head;

		// seek to the block
		while( blocks[prev_block].next_block != block ){

			prev_block = blocks[prev_block].next_block;

			ASSERT( prev_block != FFS_BLOCK_INVALID );
		}

        blocks[prev_block].next_block = blocks[block].next_block;
	}

    // check list
    if( head == &dirty_list ){

        // decrement count
        _dirty_blocks--;
    }
    else if( head == &free_list ){

        // decrement count
        _free_blocks--;
    }
}

bool ffs_block_b_is_block_free( block_t block ){

    return ffs_block_b_is_in_list( &free_list, block );
}

bool ffs_block_b_is_block_dirty( block_t block ){

    return ffs_block_b_is_in_list( &dirty_list, block );
}

bool ffs_block_b_is_in_list( block_t *head, block_t block ){

    block_t cur_block = *head;

    block_info_t *blocks = get_block_ptr();

    //ASSERT( head < _total_blocks ); // this gets checked in the loop
	ASSERT( block < (block_t)_total_blocks );

	while( cur_block != FFS_BLOCK_INVALID ){

		ASSERT( cur_block < (block_t)_total_blocks );

        if( cur_block == block ){

            return TRUE;
        }

		// advance
		cur_block = blocks[cur_block].next_block;
	}

    return FALSE;
}

// return size of list
uint8_t ffs_block_u8_list_size( block_t *head ){

    block_t cur_block = *head;
    uint8_t count = 0;
    block_info_t *blocks = get_block_ptr();

    while( cur_block != FFS_BLOCK_INVALID ){

		ASSERT( cur_block < (block_t)_total_blocks );

		// advance
		cur_block = blocks[cur_block].next_block;

        count++;
	}

    return count;
}

block_t ffs_block_fb_get_block( block_t *head, uint8_t index ){

	ASSERT( index < _total_blocks );

    block_t cur_block = *head;
    block_info_t *blocks = get_block_ptr();

    // iterate until we get to the block we want, or the end of the list
    while( ( index > 0 ) && ( cur_block != FFS_BLOCK_INVALID ) ){

        cur_block = blocks[cur_block].next_block;

        index--;
    }

    return cur_block;
}

block_t ffs_block_fb_next( block_t block ){

	ASSERT( block < (block_t)_total_blocks );
    block_info_t *blocks = get_block_ptr();

    return blocks[block].next_block;
}


uint8_t ffs_block_u8_read_flags( block_t block ){

    ASSERT( block < (block_t)_total_blocks );

    uint8_t tries = FFS_IO_ATTEMPTS;

    while( tries > 0 ){

        tries--;

        // read both sets of flags
        uint8_t flags0 = flash25_u8_read_byte( FFS_META_0( block ) + offsetof(ffs_block_meta_t, flags) );
        uint8_t flags1 = flash25_u8_read_byte( FFS_META_1( block ) + offsetof(ffs_block_meta_t, flags) );

        // check for match
        if( flags0 == flags1 ){

            return flags0;
        }
    }

    return FFS_FLAGS_INVALID; // this indicates flags are invalid
}

void ffs_block_v_set_dirty( block_t block ){

    ASSERT( block < (block_t)_total_blocks );

    // read flags
    uint8_t flags = ffs_block_u8_read_flags( block );

    // set dirty
    flags &= ~FFS_FLAG_DIRTY;

    // write new flags byte to both headers
    flash25_v_write_byte( FFS_META_0( block ) + offsetof(ffs_block_meta_t, flags), flags );
    flash25_v_write_byte( FFS_META_1( block ) + offsetof(ffs_block_meta_t, flags), flags );

    // read back to test
    // we'll accept just one flags being correct, since either way will result in a block erasure
    if( ( flash25_u8_read_byte( FFS_META_0( block ) + offsetof(ffs_block_meta_t, flags) ) != flags ) &&
        ( flash25_u8_read_byte( FFS_META_1( block ) + offsetof(ffs_block_meta_t, flags) ) != flags ) ){

        // neither set of flags are correct
        // possible write error on this block

        // try setting flag to invalid, and hope for the best
        flash25_v_write_byte( FFS_META_0( block ) + offsetof(ffs_block_meta_t, flags), FFS_FLAGS_INVALID );
        flash25_v_write_byte( FFS_META_1( block ) + offsetof(ffs_block_meta_t, flags), FFS_FLAGS_INVALID );
    }

    // add to the dirty list
	ffs_block_v_add_to_list( &dirty_list, block );
}

bool ffs_block_b_is_dirty( const ffs_block_meta_t *meta ){

    return ( ( ~meta->flags & FFS_FLAG_DIRTY ) != 0 );
}

int8_t ffs_block_i8_erase( block_t block ){

    // check if we have the index for this block loaded
    if( index_cache.block == block ){

        index_cache.block = FFS_BLOCK_INVALID;
    }

    // enable writes
    flash25_v_write_enable();

    // erase it
    flash25_v_erase_4k( FFS_BLOCK_ADDRESS( block ) );

    // spin lock until erase is finished
    BUSY_WAIT( flash25_b_busy() );

    // add to free list
    ffs_block_v_add_to_list( &free_list, block );

    return FFS_STATUS_OK;
}

int8_t ffs_block_i8_read_meta( block_t block, ffs_block_meta_t *meta ){

    ffs_block_meta_t check_meta;
    ffs_block_meta_t check_meta_1;

    ASSERT( block < (block_t)_total_blocks );

    uint8_t tries = FFS_IO_ATTEMPTS;

    while( tries > 0 ){

        tries--;

        // read both headers
        flash25_v_read( FFS_META_0(block), &check_meta, sizeof(ffs_block_meta_t) );
        flash25_v_read( FFS_META_1(block), &check_meta_1, sizeof(ffs_block_meta_t) );

        // compare
        if( memcmp( &check_meta, &check_meta_1, sizeof(ffs_block_meta_t) ) != 0 ){

            // soft error, we'll retry
            ffs_block_v_soft_error();

            continue;
        }

        // sanity checks
        if( ( check_meta.file_id >= FFS_MAX_FILES ) ||
            ( check_meta.block >= _total_blocks ) ){

            // header has bad data
            ffs_block_v_soft_error();

            continue;
        }

        // header is ok
        // check if caller wants the actual data
        if( meta != 0 ){

            *meta = check_meta;
        }

        return FFS_STATUS_OK;
    }

    // hard error, could not reliably read header
    ffs_block_v_hard_error();

    return FFS_STATUS_ERROR;
}

int8_t ffs_block_i8_write_meta( block_t block, const ffs_block_meta_t *meta ){

    // write to flash
    flash25_v_write( FFS_META_0(block), meta, sizeof(ffs_block_meta_t) );
    flash25_v_write( FFS_META_1(block), meta, sizeof(ffs_block_meta_t) );

    // read back to test
    if( ffs_block_i8_read_meta( block, 0 ) < 0 ){

        return FFS_STATUS_ERROR;
    }

    return FFS_STATUS_OK;
}


int8_t ffs_block_i8_read_index( block_t block, ffs_block_index_t **index ){

    uint16_t crc0, crc1;

    ASSERT( block < (block_t)_total_blocks );

    // check cache
    if( block == index_cache.block ){

        *index = &index_cache.index;

        return FFS_STATUS_OK;
    }

    uint8_t tries = FFS_IO_ATTEMPTS;

    while( tries > 0 ){

        tries--;

        // read both indexes
        flash25_v_read( FFS_INDEX_0(block), &index_cache.index, sizeof(index_cache.index) );
        crc0 = crc_u16_block( (uint8_t *)&index_cache.index, sizeof(index_cache.index) );

        flash25_v_read( FFS_INDEX_1(block), &index_cache.index, sizeof(index_cache.index) );
        crc1 = crc_u16_block( (uint8_t *)&index_cache.index, sizeof(index_cache.index) );

        // compare
        if( crc0 != crc1 ){

            // soft error, we'll retry
            ffs_block_v_soft_error();

            continue;
        }

        index_cache.block = block;

        *index = &index_cache.index;

        return FFS_STATUS_OK;
    }

    // hard error, could not reliably read index
    ffs_block_v_hard_error();

    return FFS_STATUS_ERROR;
}

int8_t ffs_block_i8_set_index_entry( block_t block, uint8_t logical_index, uint8_t phy_index ){

    ASSERT( logical_index < FFS_DATA_PAGES_PER_BLOCK );
    ASSERT( phy_index < FFS_PAGES_PER_BLOCK );

    // check if this index is cached
    if( block == index_cache.block ){

        index_cache.index.page_index[phy_index] = logical_index;
    }

    // write index entry
    flash25_v_write_byte( FFS_INDEX_0(block) + phy_index, logical_index );
    flash25_v_write_byte( FFS_INDEX_1(block) + phy_index, logical_index );

    // read back
    if( ( flash25_u8_read_byte( FFS_INDEX_0(block) + phy_index ) != logical_index ) ||
        ( flash25_u8_read_byte( FFS_INDEX_1(block) + phy_index ) != logical_index ) ){

        return FFS_STATUS_ERROR;
    }

    return FFS_STATUS_OK;
}

int8_t ffs_block_i8_get_index_info( block_t block, ffs_index_info_t *info ){

    ffs_block_index_t *index;

    // read index
    if( ffs_block_i8_read_index(block, &index ) < 0 ){

        return FFS_STATUS_ERROR;
    }

    // init info
    info->data_pages        = 0;
    info->free_pages        = 0;
    info->phy_last_page     = -1;
    info->logical_last_page = -1;
    info->phy_next_free     = -1;

    // scan index
    for( uint8_t i = 0; i < FFS_PAGES_PER_BLOCK; i++ ){

        // check if entry is free
        if( index->page_index[i] == 0xff ){

            info->free_pages++;

            // check if we need to mark a free page
            if( info->phy_next_free < 0 ){

                info->phy_next_free = i;
            }
        }
        // check if newer last logical page is found
        else if( index->page_index[i] >= info->logical_last_page ){

            info->logical_last_page     = index->page_index[i];
            info->phy_last_page         = i;
        }
    }

    // count data pages
    info->data_pages = info->logical_last_page + 1;

    return FFS_STATUS_OK;
}

void ffs_block_v_soft_error( void ){

    flash_fs_soft_io_errors++;
}

void ffs_block_v_hard_error( void ){

    flash_fs_hard_io_errors++;

    sys_v_set_warnings( SYS_WARN_FLASHFS_HARD_ERROR );
}

#endif