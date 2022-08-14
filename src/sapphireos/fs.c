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

#include "system.h"
#include "keyvalue.h"

#include "timers.h"
#include "threading.h"
#include "memory.h"
#include "eeprom.h"
#include "flash_fs.h"
#include "flash25.h"

#include <string.h>

#include "fs.h"


// KV:
static int8_t fs_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

        uint32_t a = 0;

        if( hash == __KV__fs_free_space ){

            a = ffs_u32_get_free_space();
        }
        else if( hash == __KV__fs_total_space ){

            a = ffs_u32_get_total_space();
        }
        else if( hash == __KV__fs_disk_files ){

            a = ffs_u32_get_file_count();
        }
        else if( hash == __KV__fs_max_disk_files ){

            a = FLASH_FS_MAX_FILES;
        }
        else if( hash == __KV__fs_virtual_files ){

            a = fs_u32_get_virtual_file_count();
        }
        else if( hash == __KV__fs_max_virtual_files ){

            a = FS_MAX_VIRTUAL_FILES;
        }
        else if( hash == __KV__fs_capacity ){

            a = flash25_u32_capacity();
        }

        memcpy( data, &a, sizeof(a) );
    }

    return 0;
}


KV_SECTION_META kv_meta_t fs_info_kv[] = {
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_free_space" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_total_space" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_disk_files" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_max_disk_files" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_virtual_files" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_max_virtual_files" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, fs_i8_kv_handler,  "fs_capacity" },
};


// internal types:

typedef struct{
	file_id_t8 file_id;
	mode_t8 mode;
	uint32_t current_pos;
} file_state_t;

typedef struct{
    PGM_P filename;
    uint32_t (*handler)( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len );
} vfile_t; // virtual file

static vfile_t vfiles[FS_MAX_VIRTUAL_FILES];

void fs_v_mount( void );

#ifdef ENABLE_FFS
static int8_t create_file_on_media( char *fname );
#endif
static int16_t write_to_media( file_id_t8 file_id, uint32_t pos, const void *ptr, uint16_t len );
static int16_t read_from_media( file_id_t8 file_id, uint32_t pos, void *ptr, uint16_t len );
static int8_t read_fname_from_media( file_id_t8 file_id, void *ptr, uint16_t max_len );
static uint32_t get_free_space_on_media( file_id_t8 file_id );
static bool media_busy( void );


// static uint16_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){
//     // NOTE: the pos and len values are already bounds checked by the FS driver

//     uint16_t ret_val = 0;

//     if( op == FS_VFILE_OP_READ ){

//         // iterate over data length and fill file info buffers as needed
//         while( len > 0 ){

//             uint8_t page = pos / sizeof(fs_file_info_t);

//             // get info page
//             fs_file_info_t info;
//             memset( &info, 0, sizeof(info) );

//             info.size = fs_i32_get_size_id( page );

//             if( info.size >= 0 ){

//                 fs_i8_get_filename_id( page, info.filename, sizeof(info.filename) );
//             }

//             // check if virtual
//             if( FS_FILE_IS_VIRTUAL( page ) ){

//                 info.flags = FS_INFO_FLAGS_VIRTUAL;
//             }

//             // get offset info page
//             uint16_t offset = pos - ( page * sizeof(info) );

//             // set copy length
//             uint16_t copy_len = sizeof(info) - offset;

//             if( copy_len > len ){

//                 copy_len = len;
//             }

//             // copy data
//             memcpy( ptr, (void *)&info + offset, copy_len );

//             // adjust pointers
//             ptr += copy_len;
//             len -= copy_len;
//             pos += copy_len;
//             ret_val += copy_len;
//         }
//     }
//     else if( op == FS_VFILE_OP_SIZE ){

//         ret_val = ( sizeof(fs_file_info_t) * FS_MAX_FILES );
//     }

//     return ret_val;
// }


// User API:

// open (and/or create a file)
// returns file handle if file found or created
// returns -1 if file not found and not created
file_t fs_f_open( char filename[], mode_t8 mode ){

    // get file ID
    file_id_t8 file_id = fs_i8_get_file_id( filename );

    // check if exists
    if( file_id >= 0 ){

        // create file handle
        mem_handle_t handle = mem2_h_alloc2( sizeof(file_state_t), MEM_TYPE_FILE_HANDLE );

        // check allocation
        if( handle < 0 ){

            return -1;
        }

        file_state_t *file_state = mem2_vp_get_ptr( handle );

        // set up file state
        file_state->file_id = file_id;
        file_state->mode = mode;

        //
        // Note that the provider of the file can ignore the mode settings
        //
        if( ( mode & FS_MODE_WRITE_APPEND ) != 0 ){

            file_state->current_pos = fs_i32_get_size_id( file_id);
        }
        else{

            file_state->current_pos = 0;
        }

        return handle;
    }

	// file not found

    #ifdef ENABLE_FFS
	// check mode
	if( ( mode & FS_MODE_CREATE_IF_NOT_FOUND ) != 0 ){

		// create file handle
		mem_handle_t handle = mem2_h_alloc2( sizeof(file_state_t), MEM_TYPE_FILE_HANDLE );

		// check allocation
		if( handle < 0 ){

			return -1;
		}

        // create file on media
        int8_t file_id = create_file_on_media( filename );

        // check if file was created
        if( file_id < 0 ){

            // file creation failed
            mem2_v_free( handle );

            return -1;
        }

		// set up file state
		file_state_t *state = mem2_vp_get_ptr( handle );

		state->file_id = file_id;
		state->mode = mode | FS_MODE_CREATED;
		state->current_pos = 0;

		return handle;
	}
    #endif

	return -1;
}

// same as fs_f_open, but with a file name from flash
file_t fs_f_open_P( PGM_P filename, mode_t8 mode ){

	// copy file name to memory
	char fname[FS_MAX_FILE_NAME_LEN];

	strlcpy_P( fname, filename, FS_MAX_FILE_NAME_LEN );

	file_t file = fs_f_open( fname, mode );

	return file;
}

file_t fs_f_open_id( file_id_t8 file_id, uint8_t mode ){

    char name[FS_MAX_FILE_NAME_LEN];

    if( fs_i8_get_filename_id( file_id, name, sizeof(name) ) < 0 ){

        return -1;
    }

    return fs_f_open( name, mode );
}

file_t fs_f_create_virtual( PGM_P filename,
                            uint32_t (*handler)( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ) ){

    for( uint8_t i = 0; i < FS_MAX_VIRTUAL_FILES; i++ ){

        if( vfiles[i].filename == 0 ){

            vfiles[i].filename  = filename;
            vfiles[i].handler   = handler;

            return i;
        }
    }

    return -1;
}

uint32_t fs_u32_get_virtual_file_count( void ){

    uint32_t count = 0;

    for( uint8_t i = 0; i < FS_MAX_VIRTUAL_FILES; i++ ){

        if( vfiles[i].filename != 0 ){

            count++;
        }
    }

    return count;
}

uint32_t fs_u32_get_file_count( void ){

    return fs_u32_get_virtual_file_count() + ffs_u32_get_file_count();
}

// returns true if the flash fs is busy.
// this only checks the flash fs - the eeprom is deprecated
// and all other media (RAM and internal flash) is read only.
bool fs_b_busy( void ){

	return media_busy();
}

// read from a file
// returns number of bytes read
int16_t fs_i16_read( file_t file, void *dst, uint16_t len ){

	// get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

    int16_t bytes_read = fs_i16_read_id( state->file_id, state->current_pos, dst, len );

    if( bytes_read > 0 ){

	    // increment current position
	    state->current_pos += bytes_read;
	}

	return bytes_read;
}

// read until an end of line, end of file, or maxlen characters
// note this function is not terribly efficient, it will read the entire buffer
// of maxlen data and then adjust the file pointer to the next byte after the newline.
//
// this function will handle newlines for LF (Mac, Linux) or CR+LF (Windows).
// it will also ensure null termination
int16_t fs_i16_readline( file_t file, void *dst, uint16_t maxlen ){

    ASSERT( maxlen > 0 ); // otherwise we crash

    maxlen--; // leave space at the end for a null terminator

    // read buffer of data from file
    uint16_t bytes_read = fs_i16_read( file, dst, maxlen );

    // get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

    // reset file position
    state->current_pos -= bytes_read;

    // find LF
    for( uint16_t i = 0; i < bytes_read; i++ ){

        if( ((char *)dst)[i] == 0x0A ){

            // reset bytes read
            bytes_read = i + 1;

            // check LF again
            if( ( bytes_read > 0 ) && ( ((char *)dst)[bytes_read - 1] == 0x0A ) ){

                // increment current position
                state->current_pos += bytes_read;
            }

            // set null termination
            ((char *)dst)[bytes_read] = 0;

            return bytes_read;
        }
    }

    // did not find terminator, or EOF
    return 0;
}

// write to a file
// returns number of bytes written
// note if the media driver is busy and is unable to immediately start a write,
// this function will return 0 bytes.  the application will need to retry
// until the system is able to complete its request
int16_t fs_i16_write( file_t file, const void *src, uint16_t len ){

	// get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

    // check read only flag
    if( state->mode & FS_MODE_READ_ONLY ){

        return -1;
    }

    uint16_t bytes_written = fs_i16_write_id( state->file_id, state->current_pos, src, len );

    if( bytes_written > 0 ){

	    // increment current position
	    state->current_pos += bytes_written;
	}

	return bytes_written;
}

// returns a file's size
int32_t fs_i32_get_size( file_t file ){

	// get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( state->file_id ) ){

        // compute vfile id
        uint8_t vfile_id = state->file_id - FLASH_FS_MAX_FILES;

        return vfiles[vfile_id].handler( FS_VFILE_OP_SIZE, 0, 0, 0 );
	}
    else{

        return ffs_i32_get_file_size( state->file_id );
    }

    return -1;
}

// get current position in a file
int32_t fs_i32_tell( file_t file ){

	// get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return FFS_STATUS_INVALID_FILE;
    }

	return state->current_pos;
}

// set current position in a file
// if position is greater than the size of the file, it
// will be set to the end
void fs_v_seek( file_t file, uint32_t pos ){

	// get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return;
    }

	if( (int32_t)pos > fs_i32_get_size( file ) ){

		pos = fs_i32_get_size( file );
	}

    state->current_pos = pos;
}

// delete a file
// some files cannot be deleted, this function will not report
// whether or not the file was deleted.
// this will NOT close the file handle!
void fs_v_delete( file_t file ){

	// get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check for invalid file
    if( state->file_id < 0 ){

        return;
    }

    // check read only flag
    if( state->mode & FS_MODE_READ_ONLY ){

        return;
    }

    fs_i8_delete_id( state->file_id );

    // set file id to invalid
    state->file_id = -1;
}

// close a file
file_t fs_f_close( file_t file ){

    // get file state
	file_state_t *state = mem2_vp_get_ptr( file );

    // check if not virtual
    if( !FS_FILE_IS_VIRTUAL( state->file_id ) ){

        // flush the file cache
        // .... if we had one
    }

	mem2_v_free( file );

	return -1; // convience for resetting local file handle to -1
}



// initialize file system
void fs_v_init( void ){

    // create vfile
    // fs_f_create_virtual( PSTR("fileinfo"), vfile );
}


/***************
* ID functions *
***************/

bool fs_b_exists_id( file_id_t8 id ){

    if( ( id >= FS_MAX_FILES ) || ( id < 0 ) ){

        return FALSE;
    }

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( id ) ){

        // check if file exists
        return ( vfiles[id - FLASH_FS_MAX_FILES].filename != 0 );
    }

    return ffs_i32_get_file_size( id ) >= 0;
}

file_id_t8 fs_i8_get_file_id( char *filename ){

    // search virtual files
    for( uint8_t i = 0; i < FS_MAX_VIRTUAL_FILES; i++ ){

        // check if file exists at this id
        if( vfiles[i].filename != 0 ){

            // compare file name
            if( strncmp_P( filename, vfiles[i].filename, FS_MAX_FILE_NAME_LEN ) == 0 ){

                return i + FLASH_FS_MAX_FILES; // virtual files offset by FLASH_FS_MAX_FILES
            }
        }
    }

    // search for file on flash file system
    for( uint8_t i = 0; i < FLASH_FS_MAX_FILES; i++ ){

        // does file at this id exist
        if( ffs_i32_get_file_size( i ) < 0 ){

            continue;
        }

        char fname[FS_MAX_FILE_NAME_LEN];

        // read file name from media
        read_fname_from_media( i, fname, sizeof(fname) );

        // compare file name
        if( strncmp( fname, filename, FS_MAX_FILE_NAME_LEN ) == 0 ){

            return i;
        }
    }

    return -1;
}

file_id_t8 fs_i8_get_file_id_P( PGM_P filename ){

    // copy file name to memory
    char fname[FS_MAX_FILE_NAME_LEN];

    strlcpy_P( fname, filename, FS_MAX_FILE_NAME_LEN );

    return fs_i8_get_file_id( fname );
}


// read a file name from given id
// return -1 if file does not exist
int8_t fs_i8_get_filename_id( file_id_t8 id, void *dst, uint16_t buf_size ){

    if( !fs_b_exists_id( id ) ){

        return -1;
    }

    // ensure at least all 0s are returned, in case the caller tries to print the buffer
    memset( dst, 0, buf_size );

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( id ) ){

        // check if file exists
        if( vfiles[id - FLASH_FS_MAX_FILES].filename != 0 ){

            strlcpy_P( dst, vfiles[id - FLASH_FS_MAX_FILES].filename, buf_size );

            return 0;
        }
        else{

            return -1;
        }
    }
    else{

        return read_fname_from_media( id, dst, buf_size );
    }

    return -1;
}

int32_t fs_i32_get_size_id( file_id_t8 id ){

    if( !fs_b_exists_id( id ) ){

        return -1;
    }

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( id ) ){

        // check if file exists
        if( vfiles[id - FLASH_FS_MAX_FILES].filename != 0 ){

            return vfiles[id - FLASH_FS_MAX_FILES].handler( FS_VFILE_OP_SIZE, 0, 0, 0 );
        }
        else{

            return -1;
        }
    }
    else{

        return ffs_i32_get_file_size( id );
    }

    return -1;
}

int8_t fs_i8_delete_id( file_id_t8 id ){

    if( !fs_b_exists_id( id ) ){

        return -1;
    }

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( id ) ){

        uint8_t vfile_id = id - FLASH_FS_MAX_FILES;

        return vfiles[vfile_id].handler( FS_VFILE_OP_DELETE, 0, 0, 0 );
    }
    else{

        // scan memory handles for file handles and invalidate the file ID for
        // any handles that have this file open.
        uint8_t index = 0;

        while(1){
            
            file_t file = mem2_h_get_handle( index, MEM_TYPE_FILE_HANDLE );

            index++;

            // guard against wraparound
            if( index == 0 ){

                break;
            }

            if( file == MEM_ERR_END_OF_LIST ){

                break;
            }

            if( file < 0 ){

                continue;
            }

            // get state
            file_state_t *state = mem2_vp_get_ptr( file );

            if( state->file_id == id ){

                // set file id to invalid
                state->file_id = -1;
            }
        }

        return ffs_i8_delete_file( id );
    }

    return -1;
}

int16_t fs_i16_read_id( file_id_t8 id, uint32_t pos, void *dst, uint16_t len ){

    if( !fs_b_exists_id( id ) ){

        return -1;
    }

    uint16_t bytes_read = 0;

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( id ) ){

        // compute vfile id
        uint8_t vfile_id = id - FLASH_FS_MAX_FILES;

        uint32_t size = vfiles[vfile_id].handler( FS_VFILE_OP_SIZE, 0, 0, 0 );

        // bounds check!
        if( pos > size ){

            return 0;
        }

        uint32_t read_len = len;

        // bounds check!
        if( ( pos + read_len ) > size ){

            read_len = size - pos;
        }

        // read file data
        bytes_read = vfiles[vfile_id].handler( FS_VFILE_OP_READ, pos, dst, read_len );

    }
    else{

        // read file data
        bytes_read = read_from_media( id, pos, dst, len );
    }

    return bytes_read;
}

int16_t fs_i16_write_id( file_id_t8 id, uint32_t pos, const void *src, uint16_t len ){

    if( !fs_b_exists_id( id ) ){

        return -1;
    }

    uint16_t bytes_written = 0;

    // check if virtual
    if( FS_FILE_IS_VIRTUAL( id ) ){

        // compute vfile id
        uint8_t vfile_id = id - FLASH_FS_MAX_FILES;

        uint32_t size = vfiles[vfile_id].handler( FS_VFILE_OP_SIZE, 0, 0, 0 );

        // bounds check!
        if( pos > size ){

            return 0;
        }

        uint32_t write_len = len;

        // bounds check!
        if( ( pos + write_len ) > size ){

            write_len = size - pos;
        }

        // write file data
        // NOTE:
        // this discards the "const" qualifier on src.
        // we're trusting that the vfile handler is properly implemented and won't trash memory on a write
        bytes_written = vfiles[vfile_id].handler( FS_VFILE_OP_WRITE, pos, (void *)src, write_len );
    }

    else{

        // write file data
        bytes_written = write_to_media( id, pos, (void *)src, len );
    }

    return bytes_written;
}

// driver functions:
#ifdef ENABLE_FFS
static int8_t create_file_on_media( char *fname ){

    int8_t file_id = ffs_i8_create_file( fname );

    return file_id;
}
#endif

// returns number of bytes committed to the device driver's write buffers,
// NOT the number that have actually been written to the device!  The drivers
// will stream data to the device as needed.
static int16_t write_to_media( file_id_t8 file_id, uint32_t pos, const void *ptr, uint16_t len ){

    // check free space on media and limit to available space
    // this must be done even if the file size won't be changing, since the
    // underlying file system requires some free space to operate.
    uint32_t free_space = get_free_space_on_media( file_id );

    // bounds check requested space
    if( len > free_space ){

        trace_printf("Insufficient free space! %d < %d\r\n", free_space, len);

        // limit to free space available
        len = free_space;
    }

    return ffs_i32_write( file_id, pos, ptr, len );
}

static int16_t read_from_media( file_id_t8 file_id, uint32_t pos, void *ptr, uint16_t len ){

	// bounds check file
    int32_t file_size = fs_i32_get_size_id( file_id );

	if( (int32_t)( pos + len ) >= file_size ){

		len = file_size - pos;
	}

    return ffs_i32_read( file_id, pos, ptr, len );
}

static int8_t read_fname_from_media( file_id_t8 file_id, void *ptr, uint16_t max_len ){

    return ffs_i8_read_filename( file_id, ptr, max_len );
}

static uint32_t get_free_space_on_media( file_id_t8 file_id ){

    uint32_t free_space;

    // special handling for firmware file
    if( file_id == FFS_FILE_ID_FIRMWARE_0 ){

        free_space = FLASH_FS_FIRMWARE_0_PARTITION_SIZE;
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_1 ){

        free_space = FLASH_FS_FIRMWARE_1_PARTITION_SIZE;
    }
    else if( file_id == FFS_FILE_ID_FIRMWARE_2 ){

        free_space = FLASH_FS_FIRMWARE_2_PARTITION_SIZE;
    }
    else{

        free_space = ffs_u32_get_free_space();
    }

    return free_space;
}

static bool media_busy( void ){

	return FALSE;
}

