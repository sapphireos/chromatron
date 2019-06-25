/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#include "flash25.h"
#include "ffs_global.h"
#include "ffs_block.h"
#include "flash_fs_partitions.h"

#include "keyvalue.h"

#include "ffs_page.h"

// #define NO_EVENT_LOGGING
#include "event_log.h"

typedef struct{
    ffs_page_t page;
    uint16_t page_number;
    ffs_file_t file_id;
    int32_t page_addr;
} page_cache_t;

typedef struct{
	block_t start_block;
	int32_t size;
} file_info_t;

static file_info_t files[FFS_MAX_FILES];

static page_cache_t page_cache;

static uint32_t flash_fs_block_copies;
static uint32_t flash_fs_page_allocs;
static uint32_t flash_fs_page_cache_hits;
static uint32_t flash_fs_page_cache_misses;

KV_SECTION_META kv_meta_t ffs_page_info_kv[] = {
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_block_copies,        0,  "flash_fs_block_copies" },
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_page_allocs,         0,  "flash_fs_page_allocs" },
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_page_cache_hits,     0,  "flash_fs_page_cache_hits" },
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_page_cache_misses,   0,  "flash_fs_page_cache_misses" },
};


static int8_t ffs_page_i8_block_copy( block_t source_block, block_t dest_block );
static int8_t ffs_page_i8_alloc_block( ffs_file_t file_id );
static int32_t ffs_page_i32_seek_page( ffs_file_t file_id, uint16_t page );



// returns TRUE if seq1 is newer than seq2
static bool compare_sequences( uint8_t seq1, uint8_t seq2 ){

    return ( (int8_t)( seq1 - seq2 ) ) > 0;
}

static int32_t page_address( block_t block, uint8_t page_index ){

    // calculate page address
    int32_t addr = FFS_BLOCK_ADDRESS( block ); // block base address
    addr += ( sizeof(ffs_block_header_t) * 2 ); // offset to first page

    // offset to page index
    addr += ( sizeof(ffs_page_t) * page_index );

    return addr;
}

static void invalidate_cache( void ){

    page_cache.file_id = -1;
    page_cache.page.len = 0;
}

void ffs_page_v_reset( void ){

    // initialize data structures
    for( uint8_t i = 0; i < FFS_MAX_FILES; i++ ){

        files[i].start_block = -1;
        files[i].size = -1;
    }
}

void ffs_page_v_init( void ){

    invalidate_cache();

    ffs_page_v_reset();

    // if( sys_u8_get_mode() == SYS_MODE_SAFE ){

    //     return;
    // }

    ffs_block_meta_t meta;

    // scan for files
    for( uint16_t i = 0; i < ffs_block_u16_total_blocks(); i++ ){

        // read header flags
        uint8_t flags = ffs_block_u8_read_flags( i );

        // check if marked as valid
        if( flags == (uint8_t)~FFS_FLAG_VALID ){

            // read meta header and check for read errors
            if( ffs_block_i8_read_meta( i, &meta ) < 0 ){

                // read error, mark as dirty
                ffs_block_v_set_dirty( i );

                continue;
            }

            // meta data verified

            // add to file list
            ffs_block_v_add_to_list( &files[meta.file_id].start_block, i );
        }

        // ignore invalid blocks, the block driver will handle those
    }

    // file data scan
    for( uint8_t file = 0; file < FFS_MAX_FILES; file++ ){

scan_start:

        // check if file exists
        if( files[file].start_block == FFS_BLOCK_INVALID ){

            continue;
        }

        // create block map for file
        block_t file_map[ffs_block_u16_total_blocks()];
        memset( file_map, FFS_BLOCK_INVALID, sizeof(file_map) );

        // iterate over file list and map block numbers
        block_t block = files[file].start_block;

        while( block != FFS_BLOCK_INVALID ){

            // read meta header.
            // we've already scanned the meta data once and verified the data is intact, so we'll
            // assume the data is valid here.
            ASSERT( ffs_block_i8_read_meta( block, &meta ) == 0 );

            // check if this block number was already found
            if( file_map[meta.block] != FFS_BLOCK_INVALID ){

                // read meta from prev block
                ffs_block_meta_t prev_meta;
                prev_meta.sequence = 0;

                // same notes as above
                ASSERT( ffs_block_i8_read_meta( block, &prev_meta ) == 0 );

                // compare sequences
                if( compare_sequences( meta.sequence, prev_meta.sequence ) ){

                    // current block is newer, set old block to dirty
                    ffs_block_v_set_dirty( file_map[meta.block] );

                    // assign new block
                    file_map[meta.block] = block;
                }
                else{

                    // we already have the most current block, set the one we just scanned to dirty
                    ffs_block_v_set_dirty( block );
                }
            }
            else{

                file_map[meta.block] = block;
            }

            // get next block
            block = ffs_block_fb_next( block );
        }

        // now we have file map and we've resolved duplicates
        // let's check for missing blocks
        // we'll treat that is a file error and delete the entire file
        for( uint16_t i = 0; i < ( ffs_block_u16_total_blocks() - 1 ); i++ ){

            // check if current block is missing but there is a next block
            if( ( file_map[i] == FFS_BLOCK_INVALID ) && ( file_map[i + 1] != FFS_BLOCK_INVALID ) ){

                // delete file
                ffs_page_i8_delete_file( file );

                goto scan_start;
            }
        }

        // no duplicate blocks, no missing blocks.  rebuild the list in order.
        files[file].start_block = FFS_BLOCK_INVALID;

        for( uint16_t i = 0; i < ffs_block_u16_total_blocks(); i++ ){

            // check for end of file
            if( file_map[i] == FFS_BLOCK_INVALID ){

                break;
            }

            // add to list
            ffs_block_v_add_to_list( &files[file].start_block, file_map[i] );
        }

        // set file size
        files[file].size = ffs_page_i32_scan_file_size( file );

        // check for error
        if( files[file].size < 0 ){

            ffs_page_i8_delete_file( file );
        }

        // scan file data
        if( ffs_page_i8_file_scan( file ) < 0 ){

            // data integrity error, trash file
            ffs_page_i8_delete_file( file );

            goto scan_start;
        }
    }
}

uint8_t ffs_page_u8_count_files( void ){

    uint8_t count = 0;

    for( uint8_t i = 0; i < FFS_MAX_FILES; i++ ){

        if( files[i].size >= 0 ){

            count++;
        }
    }

    return count;
}

int8_t ffs_page_i8_file_scan( ffs_file_t file_id ){

    ASSERT( file_id < FFS_MAX_FILES );

    // calculate number of pages
    uint16_t pages = files[file_id].size / FFS_PAGE_DATA_SIZE;

    if( ( files[file_id].size % FFS_PAGE_DATA_SIZE ) > 0 ){

        pages++;
    }

    // special case, file is empty
    if( files[file_id].size == 0 ){

        // check that we don't have a page 0
        if( ffs_page_i32_seek_page( file_id, 0 ) != FFS_STATUS_EOF ){

            return FFS_STATUS_ERROR;
        }

        return FFS_STATUS_OK;
    }

    // iterate over all pages and verify all data
    for( uint16_t i = 0; i < pages; i++ ){

        if( ffs_page_i8_read( file_id, i ) < 0 ){

            return FFS_STATUS_ERROR;
        }

        // check length (except on last page)
        if( i < ( pages - 1 ) ){

            if( page_cache.page.len != FFS_PAGE_DATA_SIZE ){

                return FFS_STATUS_ERROR;
            }
        }
    }

    return FFS_STATUS_OK;
}


int32_t ffs_page_i32_scan_file_size( ffs_file_t file_id ){

    ASSERT( file_id < FFS_MAX_FILES );

    uint8_t block_count = ffs_block_u8_list_size( &files[file_id].start_block );

    ASSERT( block_count > 0 );

    uint8_t file_block_number = block_count - 1;

    ffs_index_info_t index_info;

    block_t block = ffs_block_fb_get_block( &files[file_id].start_block, file_block_number );

    // check if block was found
    if( block == FFS_BLOCK_INVALID ){

        // file is empty
        return 0;
    }

    // read index info for last block in file
    if( ffs_block_i8_get_index_info( block, &index_info ) <  0 ){

        return FFS_STATUS_ERROR;
    }

    // check if there are data pages in this block
    if( index_info.data_pages == 0 ){

        // check if there is a previous block to try
        if( file_block_number > 0 ){

            // try previous block
            file_block_number--;
            block = ffs_block_fb_get_block( &files[file_id].start_block, file_block_number );

            ASSERT( block != FFS_BLOCK_INVALID );

            if( ffs_block_i8_get_index_info( block, &index_info ) <  0 ){

                return FFS_STATUS_ERROR;
            }
        }
    }

    // check last page
    if( index_info.logical_last_page < 0 ){

        // file is empty
        return 0;
    }

    // calculate logical file page number
    uint16_t last_page = ( file_block_number * FFS_DATA_PAGES_PER_BLOCK ) + index_info.logical_last_page;

    if( ffs_page_i8_read( file_id, last_page ) < 0 ){

        return FFS_STATUS_ERROR;
    }

    // return calculated file size
    return ( (uint32_t)last_page * (uint32_t)FFS_PAGE_DATA_SIZE ) + (uint32_t)page_cache.page.len;
}


ffs_file_t ffs_page_i8_create_file( void ){

    ffs_file_t file = -1;

    // get a free file id
    for( uint8_t i = 0; i < FFS_MAX_FILES; i++ ){

        // check if ID is free
        if( files[i].size < 0 ){

            // assign ID
            file = i;

            break;
        }
    }

    // was there a free ID?
    if( file < 0 ){

        return FFS_STATUS_NO_FREE_FILES;
    }

    // allocate a block
    int8_t status = ffs_page_i8_alloc_block( file );

    // check status
    if( status < 0 ){

        return status; // error code
    }

    // set file size
    files[file].size        = 0;

    // return file ID
    return file;
}

int8_t ffs_page_i8_delete_file( ffs_file_t file_id ){

    ASSERT( file_id < FFS_MAX_FILES );

    // set all file blocks to dirty
    block_t block = ffs_block_fb_get_block( &files[file_id].start_block, 0 );

    while( block != FFS_BLOCK_INVALID ){

        // get next block first
        block_t next = ffs_block_fb_next( block );

        ffs_block_v_set_dirty( block );

        block = next;
    }

    // clear file entry
    files[file_id].start_block  = FFS_BLOCK_INVALID;
    files[file_id].size         = -1;

    return FFS_STATUS_OK;
}

int32_t ffs_page_i32_file_size( ffs_file_t file_id ){

    ASSERT( file_id < FFS_MAX_FILES );

    return files[file_id].size;
}

ffs_page_t* ffs_page_p_get_cached_page( void ){

    // ensure page is valid
    ASSERT( page_cache.file_id >= 0 );

    return &page_cache.page;
}

int8_t ffs_page_i8_read( ffs_file_t file_id, uint16_t page ){

    // check cache
    if( ( page_cache.file_id == file_id ) && ( page_cache.page_number == page ) ){

        flash_fs_page_cache_hits++;

        return FFS_STATUS_OK;
    }

    flash_fs_page_cache_misses++;

    // trash the cache
    invalidate_cache();

    // seek to page
    int32_t page_addr = ffs_page_i32_seek_page( file_id, page );

    // check error codes
    if( page_addr == FFS_STATUS_EOF ){

        return FFS_STATUS_EOF;
    }
    else if( page_addr < 0 ){

        return FFS_STATUS_ERROR;
    }

    // ok, we have the correct page address

    // set up retry loop
    uint8_t tries = FFS_IO_ATTEMPTS;

    while( tries > 0 ){

        tries--;

        // read page
        flash25_v_read( page_addr, &page_cache.page, sizeof(ffs_page_t) );

        // check crc
        if( crc_u16_block( page_cache.page.data, page_cache.page.len ) == page_cache.page.crc ){

            // set up cache
            page_cache.page_number = page;
            page_cache.page_addr = page_addr;
            page_cache.file_id = file_id;

            return FFS_STATUS_OK;
        }

        ffs_block_v_soft_error();
    }

    ffs_block_v_hard_error();

    return FFS_STATUS_ERROR;
}

int8_t ffs_page_i8_write( ffs_file_t file_id, uint16_t page, uint8_t offset, const void *data, uint8_t len ){

    ASSERT( file_id < FFS_MAX_FILES );
    ASSERT( page < ffs_page_u16_total_pages() );

    // calculate logical block number
    block_t block = page / FFS_DATA_PAGES_PER_BLOCK;
    uint8_t page_index = page % FFS_DATA_PAGES_PER_BLOCK;

    // get list size
    uint8_t block_count = ffs_block_u8_list_size( &files[file_id].start_block );

    // set page count, if file has a size greater than 0
    uint16_t page_count = 0;

    if( files[file_id].size > 0 ){

        page_count = ( files[file_id].size / FFS_PAGE_DATA_SIZE ) + 1;
    }

    ASSERT( page <= page_count );


    // set up retry loop
    uint8_t tries = FFS_IO_ATTEMPTS;

    while( tries > 0 ){

        tries--;

        flash_fs_page_allocs++;


        // check if we're trying to write the next logical block
        if( block_count == block ){
            // Ex: we have 2 blocks, and we want a 3rd block (number 2)

            // allocate a new block
            if( ffs_page_i8_alloc_block( file_id ) != FFS_STATUS_OK ){

                continue;
            }
        }

        // seek physical block number in file block list
        block_t phy_block = ffs_block_fb_get_block( &files[file_id].start_block, block );

        ASSERT( phy_block != FFS_BLOCK_INVALID );

        // get index info for block
        ffs_index_info_t index_info;

        if( ffs_block_i8_get_index_info( phy_block, &index_info ) < 0 ){

            continue;
        }

        // check for free page
        if( index_info.phy_next_free < 0 ){

            // this block is full
            // try replacing with a new block
            phy_block = ffs_page_i16_replace_block( file_id, block );

            // check error code
            if( phy_block == FFS_BLOCK_INVALID ){

                continue;
            }

            // get new index info
            if( ffs_block_i8_get_index_info( phy_block, &index_info ) < 0 ){

                continue;
            }
        }

        if( index_info.phy_next_free < 0 ){

            continue;
        }

        // index update succeeded,
        int32_t page_addr = page_address( phy_block, index_info.phy_next_free );

        // read page into cache
        int8_t page_read_status = ffs_page_i8_read( file_id, page );

        // check for errors, but filter on EOF.  EOF is ok.
        if( ( page_read_status < 0 ) &&
            ( page_read_status != FFS_STATUS_EOF ) ){

            continue;
        }

        uint8_t write_len = FFS_PAGE_DATA_SIZE;

        // bounds check on requested length
        if( (uint32_t)write_len > len ){

            write_len = len;
        }

        // bounds check on page size and offset
        if( write_len > ( FFS_PAGE_DATA_SIZE - offset ) ){

            write_len = ( FFS_PAGE_DATA_SIZE - offset );
        }


        // check if EOF, in this case, we're writing to a new
        // page.  we want to prefill with 1s.
        if( page_read_status == FFS_STATUS_EOF ){

            memset( &page_cache.page.data, 0xff, sizeof(page_cache.page.data) );
        }

        // copy data into page
        memcpy( &page_cache.page.data[offset], data, write_len );

        // check if page is increasing in size
        if( page_cache.page.len < ( offset + write_len ) ){

            page_cache.page.len = offset + write_len;
        }

        // calculate CRC
        page_cache.page.crc = crc_u16_block( page_cache.page.data, page_cache.page.len );

        // write to flash
        flash25_v_write( page_addr, &page_cache.page, sizeof(ffs_page_t) );

        // update index
        if( ffs_block_i8_set_index_entry( phy_block, page_index, index_info.phy_next_free ) < 0 ){

            continue;
        }

        // trash the cache so we force a reread and CRC check
        invalidate_cache();

        if( ffs_page_i8_read( file_id, page ) == FFS_STATUS_OK ){

            // calculate file length up to this page plus the data in it
            uint32_t file_length_to_here = ( (uint32_t)page * (uint32_t)FFS_PAGE_DATA_SIZE ) + page_cache.page.len;

            // check file size
            if( file_length_to_here > (uint32_t)files[file_id].size ){

                // adjust file size
                files[file_id].size = file_length_to_here;
            }

            // success
            return FFS_STATUS_OK;
        }

        ffs_block_v_soft_error();
    }

    ffs_block_v_hard_error();

    return FFS_STATUS_ERROR;
}


// allocate a new block and add it to the end of the file
static int8_t ffs_page_i8_alloc_block( ffs_file_t file_id ){

    ASSERT( file_id < FFS_MAX_FILES );

    // get current block count for file
    uint8_t block_count = ffs_block_u8_list_size( &files[file_id].start_block );

    // allocate a block to the file
    block_t block = ffs_block_fb_alloc();

    // check if there was a free block
    if( block == FFS_BLOCK_INVALID ){

        return FFS_STATUS_NO_FREE_SPACE;
    }

    // set up block header
    ffs_block_meta_t meta;
    meta.file_id    = file_id;
    meta.flags      = ~FFS_FLAG_VALID;
    meta.block      = block_count;
    meta.sequence   = 0;
    memset( meta.reserved, 0xff, sizeof(meta.reserved) );

    // write meta data to block and check for errors
    if( ffs_block_i8_write_meta( block, &meta ) != 0 ){

        // write error, set block to dirty
        ffs_block_v_set_dirty( block );

        return FFS_STATUS_ERROR;
    }

    // add to file list
    ffs_block_v_add_to_list( &files[file_id].start_block, block );

    // success
    return FFS_STATUS_OK;
}

block_t ffs_page_i16_replace_block( ffs_file_t file_id, uint8_t file_block ){

    ASSERT( file_id < FFS_MAX_FILES );

    // map logical file block to physical block
    block_t phy_block = ffs_block_fb_get_block( &files[file_id].start_block, file_block );

    ASSERT( phy_block != FFS_BLOCK_INVALID );

    // try allocating a new block
    block_t new_block = ffs_block_fb_alloc();

    if( new_block == FFS_BLOCK_INVALID ){

        return FFS_STATUS_NO_FREE_SPACE;
    }

    // copy old block to new
    if( ffs_page_i8_block_copy( phy_block, new_block ) < 0 ){

        // fail
        ffs_block_v_set_dirty( new_block );

        return FFS_STATUS_ERROR;
    }

    // replace block in file list
    ffs_block_v_replace_in_list( &files[file_id].start_block, phy_block, new_block );

    // set old block to dirty
    ffs_block_v_set_dirty( phy_block );

    // return new phy block
    return new_block;
}


static int8_t ffs_page_i8_block_copy( block_t source_block, block_t dest_block ){

    ffs_block_meta_t meta;

    flash_fs_block_copies++;

    // read source meta
    if( ffs_block_i8_read_meta( source_block, &meta ) < 0 ){

        // read error
        return FFS_STATUS_ERROR;
    }

    // calc base page number
    uint16_t base_page = meta.block * FFS_DATA_PAGES_PER_BLOCK;

    // iterate through source pages
    for( uint8_t i = 0; i < FFS_DATA_PAGES_PER_BLOCK; i++ ){

        // read page
        int8_t status = ffs_page_i8_read( meta.file_id, base_page + i );

        // check EOF
        if( status == FFS_STATUS_EOF ){

            goto done;
        }

        // check error
        if( status < 0 ){

            return FFS_STATUS_ERROR;
        }

        // update dest index
        if( ffs_block_i8_set_index_entry( dest_block, i, i ) < 0 ){

            return FFS_STATUS_ERROR;
        }

        // write page data
        flash25_v_write( page_address( dest_block, i ), &page_cache.page, sizeof(page_cache.page) );

        // read back to verify
        if( ffs_page_i8_read( meta.file_id, base_page + i ) != FFS_STATUS_OK ){

            return FFS_STATUS_ERROR;
        }
    }

done:
    // increment block sequence
    meta.sequence++;

    // write meta
    if( ffs_block_i8_write_meta( dest_block, &meta ) < 0 ){

        return FFS_STATUS_ERROR;
    }

    return FFS_STATUS_OK;
}


static int32_t ffs_page_i32_seek_page( ffs_file_t file_id, uint16_t page ){

    ASSERT( file_id < FFS_MAX_FILES );
    ASSERT( page < ffs_page_u16_total_pages() );

    // calculate logical block number
    uint8_t block = page / FFS_DATA_PAGES_PER_BLOCK;
    uint8_t page_index = page % FFS_DATA_PAGES_PER_BLOCK;

    // seek physical block number in file block list
    block_t phy_block = ffs_block_fb_get_block( &files[file_id].start_block, block );

    // check if block was found
    if( phy_block == FFS_BLOCK_INVALID ){

        // assume this is end of file
        return FFS_STATUS_EOF;
    }

    ffs_block_index_t *index;

    // read index from block
    if( ffs_block_i8_read_index( phy_block, &index ) < 0 ){

        // read error
        return FFS_STATUS_ERROR;
    }

    int8_t i = FFS_PAGES_PER_BLOCK - 1;

    // scan index backwards to get current physical page
    while( i >= 0 ){

        if( index->page_index[i] == page_index ){

            // calculate page address
            return page_address( phy_block, i );
        }

        i--;
    }

    // page not found, assume end of file
    return FFS_STATUS_EOF;
}
