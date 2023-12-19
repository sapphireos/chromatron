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


/*


Flash FS File Layer Caching:

Add a per-file cache, with an overall cache controller to limit memory usage.

Cache for the file should start at the multiple of the page size of the 
requested file address.  Depending on availability of free cache pages,
additional flash pages can be read ahead and stored for access.

This scheme most likely helps with write heavy loads that would write
chunks of data large enough to span multiple pages and offset enough that
multiple pages are rewritten on each write.  The cache layer would dramatically
reduce the write load.

Sequential reads that are not page aligned should also be accelerated.

This read ahead cache could change the maximum cache size based on usage:
files opened for read only would only receive up to a single cached page,
or some other limit that makes sense.
Read-ahead caching might be useful because it can increase the hit rate
on the block index cache.

Files opened for write would be allowed up to N (maybe 4 for starters).
These pages would not be allocated until an actual write was performed.


This should likely be simpler than the previously attempted but
uncompleted flash fs page cache layer that operated
at the flash translation system level.

The API surface is much simpler: the process to write a flash 
layer page is *dramatically* more complex than the file level.

Additionally, that layer does already have a single page cache 
layer - the difficulty was in increasing the size of that cache.
The addition of a file layer cache should perform the same
optimization, while possibly offering improvements due
to the increased context information available at the
file level.


Something to avoid:
It is tempting to try to integrate with the memory
manager - allocate all unused space for fs caching
and the memory manager can force a partial flush and
reclaim as much space as it needs on an allocation
failure.

The problem is that this can be tricky to get right
and catastrophic if it doesn't work perfectly.

Memory allocations that force a cache flush invoke
a very complex pathway of behavior.  All of that 
behavior must occur synchronously, must ultimately
succeed and in no way fail, and without touching 
the memory manager in any way.


The cache must be flushable at any time.  Since the
system can lose power at any time without a clean
shutdown, the cache should flush to hardware every
few seconds.

Per-file and overall statistics could be kept - and
stored in a special file perhaps.

The per-file stats are especially interesting, as
this would also record usage patterns in addition
to cache performance.

NOTE:
Flushes must write cached pages from a given file
in sequential order!




Additional notes:

It would be nice to cache file names.  A file listing is an expensive operation:
each file requires a page seek, index scan, and then a page read.  A file list
operation would likely result in invalidating all cached pages from the above 
caching scheme.

This feature should be optional: low memory targets are better off taking the 
speed hit, but the ESP based devices have enough memory.



*/


#include "cpu.h"
#include "system.h"
#include "timers.h"

#include "flash25.h"
#include "ffs_fw.h"
#include "ffs_page.h"
#include "ffs_block.h"
#include "ffs_gc.h"
#include "ffs_global.h"
#include "flash_fs_partitions.h"

#include "flash_fs.h"

#include "keyvalue.h"

// #define NO_EVENT_LOGGING
#include "event_log.h"

static bool ffs_fail;
static uint8_t board_type;

#ifdef FLASH_FS_TIMING
static uint32_t flash_fs_timing_block_init;
static uint32_t flash_fs_timing_page_init;

uint32_t flash_fs_boot_time;

KV_SECTION_META kv_meta_t flash_fs_info_kv[] = {
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_boot_time,         0,  "flash_fs_timing_boot_time" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_timing_block_init, 0,  "flash_fs_timing_block_init" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_fs_timing_page_init,  0,  "flash_fs_timing_page_init" },
};

#endif

#ifdef FLASH_FS_CACHE

typedef struct{
    ffs_file_t file_id;
    uint16_t page;
    uint8_t data[FFS_PAGE_DATA_SIZE];
    uint8_t len;
    uint32_t age;
} cache_entry_t;

static mem_handle_t cache_h;


void init_cache( void ){


}

cache_entry_t get_cache_entry( ffs_file_t file, uint16_t page ){

    return 0;
}

void flush_file( ffs_file_t file ){


}

void flush_all( void ){


}

// void invalidate_file( ffs_file_t file ){

// }

int8_t update_cache( ffs_file_t file, uint16_t page, uint8_t *data, uint8_t len ){

    return -1;
}

#endif



void ffs_v_init( void ){

    #ifdef ENABLE_FFS

    COMPILER_ASSERT( sizeof(ffs_file_meta0_t) == FFS_PAGE_DATA_SIZE );
    COMPILER_ASSERT( sizeof(ffs_file_meta1_t) == FFS_PAGE_DATA_SIZE );

    // check if external flash is present and accessible
    if( flash25_u32_capacity() == 0 ){

        sys_v_set_warnings( SYS_WARN_FLASHFS_FAIL );
        ffs_fail = TRUE;

        trace_printf("FlashFS FAIL\r\n");

        return;
    }

    uint8_t fs_version = flash25_u8_read_byte( FLASH_FS_VERSION_ADDR );
    board_type = flash25_u8_read_byte( FLASH_FS_HW_TYPE_ADDR );

    trace_printf("FlashFS version: %d board type: %d\r\n", (int)fs_version, (int)board_type);

    // check system mode and version
    if( ( sys_u8_get_mode() == SYS_MODE_FORMAT ) ||
        ( fs_version != FFS_VERSION ) ){

        // format the file system.  this will also re-mount.
        ffs_v_format();

        // restart in normal mode
        sys_reboot();
    }

    if( ffs_fw_i8_init() < 0 ){

        trace_printf("Firmware partitions failed to mount\r\n");

        sys_v_set_warnings( SYS_WARN_FLASHFS_FAIL );
        ffs_fail = TRUE;

        return;
    }

    ffs_v_mount();

    ffs_gc_v_init();

    trace_printf("FlashFS files: %u free space: %u ver: %d\r\n", ffs_u32_get_file_count(), ffs_u32_get_free_space(), fs_version );

    #else

    // FFS disabled, just mount firmware partitions

    ffs_fw_i8_init();

    #endif
}


void ffs_v_mount( void ){

    #ifdef ENABLE_FFS

    trace_printf("Mounting FlashFS\r\n");

    #ifdef FLASH_FS_TIMING
    uint32_t start_time = tmr_u32_get_system_time_ms();
    #endif
    
    ffs_block_v_init();

    #ifdef FLASH_FS_TIMING
    flash_fs_timing_block_init = tmr_u32_elapsed_time_ms( start_time );
    #endif

    ffs_page_v_init();

    #ifdef FLASH_FS_TIMING
    flash_fs_timing_page_init = tmr_u32_elapsed_time_ms( flash_fs_timing_block_init );
    #endif

    #endif
}


void ffs_v_format( void ){

    #ifdef ENABLE_FFS

    trace_printf("Formatting FlashFS...\r\n");

	// enable writes
	flash25_v_write_enable();

    // erase block 0:
    // DO NOT UNCOMMENT FOR NORMAL OPERATION!
    // Only uncomment and build if undoing a screw-up!
    // flash25_v_unlock_block0();
    // flash25_v_erase_4k( 0 );
    // </erase block 0>

	// erase entire array
    flash25_v_erase_chip();

	// wait until finished
	SAFE_BUSY_WAIT( flash25_b_busy() );

    // check if version has not been set
    if( flash25_u8_read_byte( FLASH_FS_VERSION_ADDR ) != FFS_VERSION ){

        trace_printf("Setting FFS version in block 0\r\n");

        // note this will destroy the board rev

        // erase block 0
        flash25_v_unlock_block0();
        flash25_v_erase_4k( 0 );

        // write version
        flash25_v_unlock_block0();
        flash25_v_write_byte( FLASH_FS_VERSION_ADDR, FFS_VERSION );
    }

    // re-mount
    ffs_v_mount();
    #endif
}

uint8_t ffs_u8_read_board_type( void ){

    #ifdef BOOTLOADER
    return flash25_u8_read_byte( FLASH_FS_HW_TYPE_ADDR );
    #else
    return board_type;
    #endif
}

void ffs_v_write_board_type( uint8_t board ){

    uint8_t current_type = ffs_u8_read_board_type();

    // we can only write the board if it has not been set
    if( current_type != FLASH_FS_HW_TYPE_UNSET ){

        return;
    }

    flash25_v_unlock_block0();

    flash25_v_write_byte( FLASH_FS_HW_TYPE_ADDR, board );

    board_type = board;
}

uint32_t ffs_u32_get_file_count( void ){
    #ifdef ENABLE_FFS

    if( ffs_fail ){

        return 0;
    }

    return ffs_page_u8_count_files() + 2; // add 2 for firmware

    #else

    return 2; // add 2 for firmware

    #endif
}

uint32_t ffs_u32_get_dirty_space( void ){
    #ifdef ENABLE_FFS

    if( ffs_fail ){

        return 0;
    }

    return (uint32_t)ffs_block_u16_dirty_blocks() * (uint32_t)FFS_BLOCK_DATA_SIZE;

    #else

    return 0;

    #endif
}

uint32_t ffs_u32_get_free_space( void ){

    #ifdef ENABLE_FFS

    if( ffs_fail ){

        return 0;
    }

    return (uint32_t)ffs_block_u16_free_blocks() * (uint32_t)FFS_BLOCK_DATA_SIZE;

    #else

    return 0;

    #endif
}

uint32_t ffs_u32_get_available_space( void ){

    #ifdef ENABLE_FFS

    if( ffs_fail ){

        return 0;
    }

    return ffs_u32_get_dirty_space() + ffs_u32_get_free_space();

    #else

    return 0;

    #endif
}

uint32_t ffs_u32_get_total_space( void ){

    #ifdef ENABLE_FFS

    if( ffs_fail ){

        return 0;
    }

	return (uint32_t)ffs_block_u16_total_blocks() * (uint32_t)FFS_BLOCK_DATA_SIZE;

    #else

    return 0;

    #endif
}


int32_t ffs_i32_get_file_size( ffs_file_t file_id ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

    if( file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;    
    }
    // special case for firmware partition
    else if( file_id == FFS_FILE_ID_FIRMWARE_0 ){

        return ffs_fw_u32_size( 0 );
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){
        #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
        return ffs_fw_u32_size( 1 );
        #else
        return -1;
        #endif
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
        return ffs_fw_u32_size( 2 );
        #else
        return -1;
        #endif
    }
    
    #ifdef ENABLE_FFS
    ASSERT( file_id < FFS_MAX_FILES );

    int32_t raw_size = ffs_page_i32_file_size( file_id );

    // check for errors
    if( raw_size < 0 ){

        // return error from page driver
        return raw_size;
    }

    // check file size is valid (should have meta data)
    if( raw_size < (int32_t)FFS_FILE_META_SIZE ){

        return FFS_STATUS_ERROR;
    }

    // return data size
    return raw_size - (int32_t)FFS_FILE_META_SIZE;
    #else
    return -1;
    #endif
}

int8_t ffs_i8_read_filename( ffs_file_t file_id, char *dst, uint8_t max_len ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

    if( file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;    
    }
    // special case for firmware partition
    else if( file_id == FFS_FILE_ID_FIRMWARE_0 ){

        strlcpy_P( dst, PSTR("firmware.bin"), max_len );

        return FFS_STATUS_OK;
    }
    #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        strlcpy_P( dst, PSTR("firmware2.bin"), max_len );

        return FFS_STATUS_OK;
    }
    #endif
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        strlcpy_P( dst, PSTR("wifi_firmware.bin"), max_len );

        return FFS_STATUS_OK;
    }
    #endif

    #ifdef ENABLE_FFS

    ASSERT( file_id < FFS_MAX_FILES );

    // read meta0
    if( ffs_page_i8_read( file_id, FFS_FILE_PAGE_META_0 ) < 0 ){

        return FFS_STATUS_ERROR;
    }

    ffs_page_t *page = ffs_page_p_get_cached_page();
    ffs_file_meta0_t *meta0 = (ffs_file_meta0_t *)page->data;

    // copy filename
    strlcpy( dst, meta0->filename, max_len );

    #endif

    return FFS_STATUS_OK;
}

ffs_file_t ffs_i8_create_file( char filename[] ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

#ifdef ENABLE_FFS
    // create file
    ffs_file_t file = ffs_page_i8_create_file();

    // check status
    if( file < 0 ){

        // return error code
        return file;
    }

    // we have a valid file

    uint8_t buf[FFS_PAGE_DATA_SIZE];
    ffs_file_meta0_t *meta0 = (ffs_file_meta0_t *)buf;
    memset( meta0, 0xff, sizeof(ffs_file_meta0_t) );
    strncpy( meta0->filename, filename, FFS_FILENAME_LEN );

    // write to page 0
    if( ffs_page_i8_write( file, FFS_FILE_PAGE_META_0, 0, meta0, FFS_PAGE_DATA_SIZE  ) < 0 ){

        // write error
        goto clean_up;
    }

    ffs_file_meta1_t *meta1 = (ffs_file_meta1_t *)buf;
    memset( meta1, 0xff, sizeof(ffs_file_meta1_t) );

    // write to page 1
    if( ffs_page_i8_write( file, FFS_FILE_PAGE_META_1, 0, meta1, FFS_PAGE_DATA_SIZE  ) < 0 ){

        // write error
        goto clean_up;
    }

    // success
    return file;


// error handling
clean_up:

    // delete file
    ffs_page_i8_delete_file( file );
#endif
    return FFS_STATUS_ERROR;
}


int8_t ffs_i8_delete_file( ffs_file_t file_id ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

    if( file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;    
    }

    // special case for firmware partition
    if( file_id == FFS_FILE_ID_FIRMWARE_0 ){

        ffs_fw_v_erase( 0, FALSE );

        return FFS_STATUS_OK;
    }

    #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
    if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        ffs_fw_v_erase( 1, FALSE );

        return FFS_STATUS_OK;
    }
    #endif

    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        ffs_fw_v_erase( 2, FALSE );

        return FFS_STATUS_OK;
    }
    #endif

    #ifdef ENABLE_FFS
    ASSERT( file_id < FFS_MAX_FILES );

    return ffs_page_i8_delete_file( file_id );
    #else
    return FFS_STATUS_ERROR;
    #endif
}

int32_t ffs_i32_read( ffs_file_t file_id, uint32_t position, void *data, uint32_t len ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

    if( file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;    
    }
    // special case for firmware partition
    else if( file_id == FFS_FILE_ID_FIRMWARE_0 ){

        return ffs_fw_i32_read( 0, position, data, len );
    }
    #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        return ffs_fw_i32_read( 1, position, data, len );
    }
    #endif

    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        return ffs_fw_i32_read( 2, position, data, len );
    }
    #endif

    #ifdef ENABLE_FFS
    ASSERT( file_id < FFS_MAX_FILES );

    // get file size
    int32_t file_size = ffs_i32_get_file_size( file_id );
    
    if( file_size < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

    // check position is within file bounds
    if( position >= (uint32_t)file_size ){

        return FFS_STATUS_EOF;
    }

	int32_t total_read = 0;

    // start read loop
    while( len > 0 ){

        // calculate file page
        uint16_t file_page = ( position / FFS_PAGE_DATA_SIZE ) + FFS_FILE_PAGE_DATA_0;

        // calculate offset
        uint8_t offset = position % FFS_PAGE_DATA_SIZE;

        // read page
        if( ffs_page_i8_read( file_id, file_page ) < 0 ){

            return FFS_STATUS_ERROR;
        }

        ffs_page_t *page = ffs_page_p_get_cached_page();

        uint8_t read_len = FFS_PAGE_DATA_SIZE;

        // bounds check on requested length
        if( (uint32_t)read_len > len ){

            read_len = len;
        }

        // check that the computed offset fits within the page
        // if not, the page is corrupt
        if( offset > page->len ){

            return FFS_STATUS_ERROR;
        }

        // bounds check on page size and offset
        if( read_len > ( page->len - offset ) ){

            read_len = ( page->len - offset );
        }

        // bounds check on end of file
        if( ( file_size - position ) < read_len ){

            read_len = ( file_size - position );
        }

        // check if any data is to be read
        if( read_len == 0 ){

            return FFS_STATUS_ERROR; // something went wrong, prevent infinite loop
        }

        // copy data
        memcpy( data, &page->data[offset], read_len );

        total_read  += read_len;
        data        += read_len;
        len         -= read_len;
        position    += read_len;
    }

	return total_read;
    #else
    return 0;
    #endif
}

int32_t ffs_i32_write( ffs_file_t file_id, uint32_t position, const void *data, uint32_t len ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

    if( file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;    
    }
    // special case for firmware partition
    else if( file_id == FFS_FILE_ID_FIRMWARE_0 ){

        return ffs_fw_i32_write( 0, position, data, len );
    }
    #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        return ffs_fw_i32_write( 1, position, data, len );
    }
    #endif
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        return ffs_fw_i32_write( 2, position, data, len );
    }
    #endif

    #ifdef ENABLE_FFS

    ASSERT( file_id < FFS_MAX_FILES );

    // get file size
    int32_t file_size = ffs_i32_get_file_size( file_id );
    
    if( file_size < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

    // check position is within file bounds
    if( position > (uint32_t)file_size ){

        return FFS_STATUS_EOF;
    }

	int32_t total_written = 0;

    // start write loop
    while( len > 0 ){

        // calculate file page
        uint16_t file_page = ( position / FFS_PAGE_DATA_SIZE ) + FFS_FILE_PAGE_DATA_0;

        // calculate offset
        uint8_t offset = position % FFS_PAGE_DATA_SIZE;


        uint8_t write_len = FFS_PAGE_DATA_SIZE;

        // bounds check on requested length
        if( (uint32_t)write_len > len ){

            write_len = len;
        }

        // bounds check on page size and offset
        if( write_len > ( FFS_PAGE_DATA_SIZE - offset ) ){

            write_len = ( FFS_PAGE_DATA_SIZE - offset );
        }

        if( ffs_page_i8_write( file_id, file_page, offset, data, write_len ) != FFS_STATUS_OK ){

            // error
            return FFS_STATUS_ERROR;
        }

        total_written   += write_len;
        data            += write_len;
        len             -= write_len;
        position        += write_len;
    }

	return total_written;
    #else
    return 0;
    #endif
}

