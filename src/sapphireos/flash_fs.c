/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

// #define NO_EVENT_LOGGING
#include "event_log.h"

static bool ffs_fail;

void ffs_v_init( void ){

    #ifdef ENABLE_FFS

    COMPILER_ASSERT( sizeof(ffs_file_meta0_t) == FFS_PAGE_DATA_SIZE );
    COMPILER_ASSERT( sizeof(ffs_file_meta1_t) == FFS_PAGE_DATA_SIZE );

    // check if external flash is present and accessible
    if( flash25_u32_capacity() == 0 ){

        sys_v_set_warnings( SYS_WARN_FLASHFS_FAIL );
        ffs_fail = TRUE;

        return;
    }

    uint8_t fs_version = flash25_u8_read_byte( FLASH_FS_VERSION_ADDR );

    // check system mode and version
    if( ( sys_u8_get_mode() == SYS_MODE_FORMAT ) ||
        ( fs_version != FFS_VERSION ) ){

        // format the file system.  this will also re-mount.
        ffs_v_format();

        // restart in normal mode
        sys_reboot();
    }

    ffs_fw_i8_init();

    ffs_v_mount();

    ffs_gc_v_init();

    #endif
}


void ffs_v_mount( void ){
    #ifdef ENABLE_FFS
    ffs_block_v_init();
    ffs_page_v_init();
    #endif
}

void ffs_v_format( void ){

    #ifdef ENABLE_FFS
	// enable writes
	flash25_v_write_enable();

	// erase entire array
    flash25_v_erase_chip();

	// wait until finished
	SAFE_BUSY_WAIT( flash25_b_busy() );

    // erase block 0
    flash25_v_unlock_block0();
    flash25_v_erase_4k( 0 );

    // write version
    flash25_v_unlock_block0();
    flash25_v_write_byte( FLASH_FS_VERSION_ADDR, FFS_VERSION );

    // re-mount
    ffs_v_mount();

    #endif
}


uint32_t ffs_u32_get_file_count( void ){
    #ifdef ENABLE_FFS

    if( ffs_fail ){

        return 0;
    }

    return ffs_page_u8_count_files() + 2; // add 2 for firmware

    #else

    return 0;

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

#ifdef ENABLE_FFS
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

        return ffs_fw_u32_size( 1 );
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        return ffs_fw_u32_size( 2 );
    }

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
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        strlcpy_P( dst, PSTR("firmware2.bin"), max_len );

        return FFS_STATUS_OK;
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        strlcpy_P( dst, PSTR("wifi_firmware.bin"), max_len );

        return FFS_STATUS_OK;
    }

    ASSERT( file_id < FFS_MAX_FILES );

    // read meta0
    if( ffs_page_i8_read( file_id, FFS_FILE_PAGE_META_0 ) < 0 ){

        return FFS_STATUS_ERROR;
    }

    ffs_page_t *page = ffs_page_p_get_cached_page();
    ffs_file_meta0_t *meta0 = (ffs_file_meta0_t *)page->data;

    // copy filename
    strlcpy( dst, meta0->filename, max_len );

    return FFS_STATUS_OK;
}

ffs_file_t ffs_i8_create_file( char filename[] ){

    if( ffs_fail ){

        return FFS_STATUS_ERROR;
    }

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

    if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        ffs_fw_v_erase( 1, FALSE );

        return FFS_STATUS_OK;
    }

    if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        ffs_fw_v_erase( 2, FALSE );

        return FFS_STATUS_OK;
    }

    ASSERT( file_id < FFS_MAX_FILES );

    return ffs_page_i8_delete_file( file_id );
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
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        return ffs_fw_i32_read( 1, position, data, len );
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        return ffs_fw_i32_read( 2, position, data, len );
    }

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
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        return ffs_fw_i32_write( 1, position, data, len );
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        return ffs_fw_i32_write( 2, position, data, len );
    }


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
}
#endif
