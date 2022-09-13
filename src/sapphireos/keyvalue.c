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

#include "system.h"
#include "config.h"
#include "types.h"
#include "fs.h"
#include "sockets.h"
#include "timers.h"
#include "hash.h"
#include "ffs_fw.h"
#include "crc.h"
#include "kvdb.h"
#include "catbus_common.h"
#include "list.h"

// #define NO_LOGGING
#include "logging.h"

#include "keyvalue.h"




typedef struct  __attribute__((packed)){
    catbus_hash_t32 hash;
    uint16_t index;
} kv_hash_index_t;

#ifdef ESP32
#include "esp_spi_flash.h"

static kv_hash_index_t *ram_index;

#endif

typedef struct{
    kv_meta_t *meta;
    uint16_t count;
} kv_opt_info_t;

static list_t kv_opt_list;

static uint32_t kv_persist_writes;
static int32_t kv_test_key;
static int32_t kv_test_array[4];

static uint64_t kv_cache_hits;
static uint64_t kv_cache_misses;


static uint8_t kv_cache_index;
static kv_hash_index_t kv_cache[KV_CACHE_SIZE];

static const PROGMEM char kv_data_fname[] = "kv_data";
static file_t kv_data_file_handle = -1;

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_META_START
#else
    #define KV_SECTION_META_START       __attribute__ ((section (".kv_meta_start"), used))
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_META_END
#else
    #define KV_SECTION_META_END         __attribute__ ((section (".kv_meta_end"), used))
#endif

KV_SECTION_META_START kv_meta_t kv_start[] = {
    { CATBUS_TYPE_NONE, 0, 0, 0, 0, "kvstart" }
};

KV_SECTION_META_END kv_meta_t kv_end[] = {
    { CATBUS_TYPE_NONE, 0, 0, 0, 0, "kvend" }
};

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_OPT_START
#else
    #define KV_SECTION_OPT_START       __attribute__ ((section (".kv_opt_start"), used))
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_OPT_END
#else
    #define KV_SECTION_OPT_END         __attribute__ ((section (".kv_opt_end"), used))
#endif

KV_SECTION_OPT_START kv_meta_t kv_opt_start[] = {
    { CATBUS_TYPE_NONE, 0, 0, 0, 0, "kvoptstart" }
};

KV_SECTION_OPT_END kv_meta_t kv_opt_end[] = {
    { CATBUS_TYPE_NONE, 0, 0, 0, 0, "kvoptend" }
};


static uint16_t _kv_u16_optional_count( void ){

    uint16_t count = 0;

    list_node_t ln = kv_opt_list.head;

    while( ln >= 0 ){

        kv_opt_info_t *info = list_vp_get_data( ln );
        count += info->count;

        ln = list_ln_next( ln );
    }

    return count;
}

static int8_t _kv_i8_dynamic_count_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        if( hash == __KV__kv_dynamic_count ){
            
            STORE16(data, kvdb_u16_count());
        }
        else if( hash == __KV__kv_dynamic_db_size ){
            
            STORE16(data, kvdb_u16_db_size());
        }
        else if( hash == __KV__kv_optional_count ){
            
            STORE16(data, _kv_u16_optional_count());
        }

        return 0;
    }

    return -1;
}

KV_SECTION_META kv_meta_t kv_cfg[] = {
    { CATBUS_TYPE_UINT32,  0, 0,                   &kv_persist_writes,  0,           "kv_persist_writes" },
    { CATBUS_TYPE_INT32,   0, 0,                   &kv_test_key,        0,           "kv_test_key" },
    { CATBUS_TYPE_INT32,   3, 0,                   &kv_test_array,      0,           "kv_test_array" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, _kv_i8_dynamic_count_handler,  "kv_optional_count" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, _kv_i8_dynamic_count_handler,  "kv_dynamic_count" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, _kv_i8_dynamic_count_handler,  "kv_dynamic_db_size" },
    { CATBUS_TYPE_UINT64,  0, KV_FLAGS_READ_ONLY,  &kv_cache_hits,      0,           "kv_cache_hits" },
    { CATBUS_TYPE_UINT64,  0, KV_FLAGS_READ_ONLY,  &kv_cache_misses,    0,           "kv_cache_misses" },
};

#ifdef __SIM__
    #define SERVICE_SECTION_START
#else
    #define SERVICE_SECTION_START       __attribute__ ((section (".service_start"), used))
#endif

#ifdef __SIM__
    #define SERVICE_SECTION_END
#else
    #define SERVICE_SECTION_END         __attribute__ ((section (".service_end"), used))
#endif

SERVICE_SECTION_START kv_svc_name_t svc_start = {"sapphire.device"};
SERVICE_SECTION_END char svc_end[1] = "";

static bool persist_fail;
static bool run_persist;

typedef struct{
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    uint8_t array_len;
    uint8_t reserved[4];
} kv_persist_block_header_t;
#define KV_PERSIST_MAX_DATA_LEN     64
#define KV_PERSIST_BLOCK_LEN        ( sizeof(kv_persist_block_header_t) + KV_PERSIST_MAX_DATA_LEN )


PT_THREAD( persist_thread( pt_t *pt, void *state ) );

static int8_t _kv_i8_persist_set_internal(
    file_t f,
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len );

static int8_t _kv_i8_persist_get(
    catbus_hash_t32 hash,
    void *data,
    uint16_t len );

static uint16_t _kv_u16_fixed_count( void ){

    return ( kv_end - kv_start ) - 1;
}

static uint32_t _kv_u32_get_index_addr( void ){

    #ifdef ESP32
    return sys_u32_get_kv_index_addr() + FW_SPI_START_OFFSET;
    #else
    return sys_u32_get_kv_index_addr() + FLASH_START;
    #endif
}

uint16_t kv_u16_count( void ){

    uint16_t count = _kv_u16_fixed_count();
    
    count += _kv_u16_optional_count();

    count += kv_u8_get_dynamic_count();

    return count;
}


int16_t _kv_i16_search_cache( catbus_hash_t32 hash ){

    for( uint8_t i = 0; i < cnt_of_array(kv_cache); i++ ){

        if( kv_cache[i].hash == hash ){

            kv_cache_hits++;

            return kv_cache[i].index;
        }
    }

    kv_cache_misses++;

    return -1;
}

void _kv_v_add_to_cache( catbus_hash_t32 hash, int16_t index ){

    kv_cache[kv_cache_index].hash = hash;
    kv_cache[kv_cache_index].index = index;

    kv_cache_index++;

    if( kv_cache_index >= cnt_of_array(kv_cache) ){

        kv_cache_index = 0;
    }
}

int16_t kv_i16_search_hash( catbus_hash_t32 hash ){

    // check if hash exists
    if( hash == 0 ){

        return -1;
    }

    // check cache
    int16_t cached_index = _kv_i16_search_cache( hash );

    if( cached_index >= 0 ){

        return cached_index;
    }

    // get address of hash index
    uint32_t kv_index_start = _kv_u32_get_index_addr();

    int16_t first = 0;
    int16_t last = _kv_u16_fixed_count() - 1;
    int16_t middle = ( first + last ) / 2;

    // binary search through hash index
    while( first <= last ){

        kv_hash_index_t index_entry;
        uint32_t addr = kv_index_start + ( middle * sizeof(kv_hash_index_t) );

        #ifdef AVR
        memcpy_PF( &index_entry, addr, sizeof(index_entry) );

        #elif defined(ESP32)

        // use RAM index if available
        if( ram_index != 0 ){

            index_entry = ram_index[middle];
        }
        else{

            spi_flash_read( addr, &index_entry, sizeof(index_entry) ); 
        }
        #else

        memcpy_PF( &index_entry, (void *)addr, sizeof(index_entry) );
        #endif

        if( index_entry.hash < hash ){

            first = middle + 1;
        }
        else if( index_entry.hash == hash ){

            // update cache
            _kv_v_add_to_cache( index_entry.hash, index_entry.index );

            return index_entry.index;
        }
        else{

            last = middle - 1;
        }

        middle = ( first + last ) / 2;
    }

    // linear search through optional database
    int16_t index = 0;
    list_node_t ln = kv_opt_list.head;

    while( ln >= 0 ){

        kv_opt_info_t *info = list_vp_get_data( ln );

        kv_meta_t *meta_ptr = info->meta;
        kv_meta_t opt_meta;
        memset( &opt_meta, 0x43 ,sizeof(opt_meta));
        
        for( uint16_t i = 0; i < info->count; i++ ){

            #ifdef AVR
            memcpy_PF( &opt_meta, (uint16_t)meta_ptr, sizeof(opt_meta) );

            #else

            memcpy_PF( &opt_meta, meta_ptr, sizeof(opt_meta) );
            #endif

            if( opt_meta.hash == hash ){

                index += _kv_u16_fixed_count();

                // update cache
                _kv_v_add_to_cache( hash, index );

                return index;
            }

            index++;

            meta_ptr++;
        }

        ln = list_ln_next( ln );
    }    


    // try lookup by hash in dynamic database
    int16_t kvdb_index = kvdb_i16_get_index_for_hash( hash );

    if( kvdb_index >= 0 ){

        // offset into dynamic
        kvdb_index += ( _kv_u16_fixed_count() + _kv_u16_optional_count() );

        // update cache
        _kv_v_add_to_cache( hash, kvdb_index );

        return kvdb_index;
    }

    return KV_ERR_STATUS_NOT_FOUND;
}

void kv_v_reset_cache( void ){

    memset( kv_cache, 0, sizeof(kv_cache) );
}

int8_t lookup_index( uint16_t index, kv_meta_t *meta )
{
    // fixed KV entries:
    if( index < _kv_u16_fixed_count() ){

        // GCC is getting a bit *too* smart about looking for array bounds issues.
        // so we will workaround that here, since this is not actually out of bounds, GCC
        // just doesn't understand this is a prefilled array in .text from the linker.
        #ifdef AVR
        volatile uint16_t start_addr = (uint16_t)&kv_start;
        #else
        volatile uint32_t start_addr = (uint32_t)&kv_start;
        #endif

        kv_meta_t *start = (kv_meta_t *)start_addr;
        kv_meta_t *ptr = (kv_meta_t *)( start + 1 ) + index;

        // load meta data
        memcpy_P( meta, ptr, sizeof(kv_meta_t) );
    }
    // optional dynamically linked entries:
    else if( ( index >= _kv_u16_fixed_count() ) &&
             ( index < ( _kv_u16_fixed_count() + _kv_u16_optional_count() ) ) ){

        list_node_t ln = kv_opt_list.head;

        uint16_t sub_index = _kv_u16_fixed_count();

        while( ln >= 0 ){

            kv_opt_info_t *info = list_vp_get_data( ln );
            kv_meta_t *opt_meta = info->meta;    

            for( uint16_t i = 0; i < info->count; i++ ){

                if( sub_index == index ){

                    memcpy_P( meta, opt_meta, sizeof(kv_meta_t) );

                    goto done;
                }
                    
                opt_meta++;

                sub_index++;
            }

            ln = list_ln_next( ln );
        }
    }
    // runtime dynamic DB:
    else if( index < kv_u16_count() ){

        // compute dynamic index
        index -= ( _kv_u16_fixed_count() + _kv_u16_optional_count() );

        memset( meta, 0, sizeof(kv_meta_t) );

        catbus_meta_t catbus_meta;

        catbus_hash_t32 hash = kvdb_h_get_hash_for_index( index );

        if( hash == 0 ){

            return KV_ERR_STATUS_NOT_FOUND;            
        }

        if( kvdb_i8_get_meta( hash, &catbus_meta ) < 0 ){

            return KV_ERR_STATUS_NOT_FOUND;             
        }

        meta->handler   = 0;
        meta->hash      = catbus_meta.hash;
        meta->type      = catbus_meta.type;
        meta->flags     = catbus_meta.flags;
        meta->array_len = catbus_meta.count;
        meta->ptr       = kvdb_vp_get_ptr( hash );
    }
    else{

        return KV_ERR_STATUS_NOT_FOUND;
    }


done:
    return 0;
}

int8_t kv_i8_lookup_index( uint16_t index, kv_meta_t *meta ){

    int8_t status = lookup_index( index, meta );

    return status;
}

int8_t kv_i8_lookup_index_with_name( uint16_t index, kv_meta_t *meta ){

    int8_t status = lookup_index( index, meta );

    // static KV already loads the name.
    // check if dynamic
    if( ( index >= ( _kv_u16_fixed_count() + _kv_u16_optional_count() ) ) && ( index < kv_u16_count() ) ){

        status = kvdb_i8_lookup_name( meta->hash, meta->name );    
    }

    if( status < 0 ){

        trace_printf("idx %d  status %d fixed %u  opt %u  total %u \r\n", index, status, _kv_u16_fixed_count(), _kv_u16_optional_count(), kv_u16_count() );

    }

    return status;
}

uint16_t kv_u16_get_size_meta( kv_meta_t *meta ){

    uint16_t type_len = type_u16_size( meta->type );

    if( type_len == CATBUS_TYPE_SIZE_INVALID ){

        return CATBUS_TYPE_SIZE_INVALID;
    }

    type_len *= ( (uint16_t)meta->array_len + 1 );

    return type_len;
}   

int8_t kv_i8_lookup_hash(
    catbus_hash_t32 hash,
    kv_meta_t *meta )
{
    int16_t index = kv_i16_search_hash( hash );

    if( index < 0 ){

        return KV_ERR_STATUS_NOT_FOUND;
    }

    return kv_i8_lookup_index( index, meta );
}

int8_t kv_i8_lookup_hash_with_name(
    catbus_hash_t32 hash,
    kv_meta_t *meta )
{
    int16_t index = kv_i16_search_hash( hash );

    if( index < 0 ){

        return KV_ERR_STATUS_NOT_FOUND;
    }

    return kv_i8_lookup_index_with_name( index, meta );
}

int8_t kv_i8_get_catbus_meta( catbus_hash_t32 hash, catbus_meta_t *meta ){

    kv_meta_t kv_meta;
    if( kv_i8_lookup_hash( hash, &kv_meta ) < 0 ){

        return -1;
    }

    meta->hash      = hash;
    meta->type      = kv_meta.type;
    meta->count     = kv_meta.array_len;
    meta->flags     = kv_meta.flags;
    meta->reserved  = 0;

    return 0;
}

int8_t kv_i8_get_name( catbus_hash_t32 hash, char name[KV_NAME_LEN] ){

    kv_meta_t meta;

    memset( name, 0, KV_NAME_LEN );

    int8_t status = kv_i8_lookup_hash_with_name( hash, &meta );

    if( status == 0 ){

        strlcpy( name, meta.name, KV_NAME_LEN );
    }

    return status;
}


static int8_t _kv_i8_init_persist( void ){

    bool file_retry = TRUE;

retry:;

    file_t f = fs_f_open_P( kv_data_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return -1;
    }

    kv_persist_file_header_t header;

    // check if file was just created
    if( fs_i32_get_size( f ) == 0 ){

        header.magic = KV_PERSIST_MAGIC;
        header.version = KV_PERSIST_VERSION;

        fs_i16_write( f, &header, sizeof(header) );
        fs_v_seek( f, 0 );
    }


    memset( &header, 0, sizeof(header) );

    // read header
    fs_i16_read( f, &header, sizeof(header) );

    // check file header
    if( ( header.magic != KV_PERSIST_MAGIC ) ||
        ( header.version != KV_PERSIST_VERSION ) ){

        fs_v_delete( f );
        fs_f_close( f );

        if( file_retry ){

            file_retry = FALSE;
            goto retry;
        }
    }

    uint8_t buf[KV_PERSIST_BLOCK_LEN];
    kv_persist_block_header_t *hdr = (kv_persist_block_header_t *)buf;
    kv_meta_t meta;

    while( fs_i16_read( f, buf, sizeof(buf) ) == sizeof(buf) ){

        // look up meta data, verify type matches, and check if there is
        // a memory pointer
        // AND not an array.
        int8_t status = kv_i8_lookup_hash( hdr->hash, &meta );

        if( ( status >= 0 ) &&
            ( meta.type == hdr->type ) &&
            ( meta.ptr != 0 ) &&
            ( meta.array_len == 0 ) ){

            uint16_t type_size = kv_u16_get_size_meta( &meta );

            if( type_size > KV_PERSIST_MAX_DATA_LEN ){

                type_size = KV_PERSIST_MAX_DATA_LEN;
            }

            memcpy( meta.ptr, buf + sizeof(kv_persist_block_header_t), type_size );
        }
    }

    fs_f_close( f );

    return 0;
}


void kv_v_init( void ){

    list_v_init( &kv_opt_list );

    // check if safe mode
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        #ifdef ESP32
        uint16_t len = _kv_u16_fixed_count() * sizeof(kv_hash_index_t);

        ram_index = malloc( len );

        if( ram_index != 0 ){

            uint32_t kv_index_start = FW_SPI_START_OFFSET + sys_u32_get_fw_length() -
                                   ( (uint32_t)_kv_u16_fixed_count() * sizeof(kv_hash_index_t) );


            spi_flash_read( kv_index_start, ram_index, len );
        }
        #endif

        // open file to KV persist data
        kv_data_file_handle = fs_f_open_P( kv_data_fname, FS_MODE_READ_ONLY );

        // initialize all persisted KV items
        int8_t status = _kv_i8_init_persist();

        if( status == 0 ){

            persist_fail = FALSE;

            thread_t_create( persist_thread,
                         PSTR("kv_persist"),
                         0,
                         0 );
        }
        else{

            persist_fail = TRUE;
        }
    }
}


void kv_v_add_db_info( kv_meta_t *meta, uint16_t len ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        log_v_error_P( PSTR("Optional KV entries not available in safe mode") );

        return;
    }

    uint16_t count = len / sizeof(kv_meta_t);
    uint16_t check = len % sizeof(kv_meta_t);

    // ensure the metadata length is correct
    if( check != 0 ){

        log_v_critical_P( PSTR("fatal KV error") );
        return;
    }

    // ensure the metadata length is correct
    if( count == 0 ){

        log_v_critical_P( PSTR("fatal KV error, invalid count.  len: %u"), len );
        return;
    }

    kv_opt_info_t info = {
        meta,  
        count,
    };

    list_node_t ln = list_ln_create_node2( &info, sizeof(info), MEM_TYPE_KV_OPT );

    if( ln < 0 ){

        log_v_critical_P( PSTR("fatal KV error") );
        return;
    }
    
    list_v_insert_tail( &kv_opt_list, ln );

    kv_v_reset_cache();

    // load items for persist storage, if available
    for( uint16_t i = 0; i < count; i++ ){

        kv_meta_t opt_meta;
        memcpy( &opt_meta, meta, sizeof(opt_meta) );

        if( opt_meta.ptr == 0 ){

            continue;
        }

        uint16_t size = kv_u16_get_size_meta( &opt_meta );

        if( size == CATBUS_TYPE_SIZE_INVALID ){

            continue;
        }

        _kv_i8_persist_get( opt_meta.hash, opt_meta.ptr, size );

        meta++;
    }
}


static int8_t _kv_i8_persist_set_internal(
    file_t f,
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    if( persist_fail ){

        return 0;
    }

    // check that we aren't persisting an array
    if( meta->array_len > 0 ){

        return 0;
    }

    uint8_t buf[KV_PERSIST_MAX_DATA_LEN];

    kv_persist_block_header_t hdr;

    fs_v_seek( f, sizeof(kv_persist_file_header_t) );

    // seek to matching item or end of file
    while( fs_i16_read( f, &hdr, sizeof(hdr) ) == sizeof(hdr) ){

        // check for match
        if( hdr.hash == hash ){

            // remember current position
            int32_t pos = fs_i32_tell( f );

            // found our match
            // copy data into buffer
            fs_i16_read( f, buf, len );

            // compare file data against the data we've been given
            if( memcmp( buf, data, len ) == 0 ){

                // data has not changed, so there's no reason to write anything

                goto end;
            }

            // back up the file position to before header
            fs_v_seek( f, pos - sizeof(hdr) );

            break;
        }

        // advance position to next entry
        fs_v_seek( f, fs_i32_tell( f ) + KV_PERSIST_MAX_DATA_LEN );
    }

    // set up header
    hdr.hash        = hash;
    hdr.type        = meta->type;
    hdr.array_len   = meta->array_len;
    memset( hdr.reserved, 0, sizeof(hdr.reserved) );

    // write header
    fs_i16_write( f, &hdr, sizeof(hdr) );

    // bounds check data
    // not needed, we assert above
    // if( len > KV_PERSIST_BLOCK_DATA_LEN ){

    //     len = KV_PERSIST_BLOCK_DATA_LEN;
    // }

    // Copy data to 0 initialized buffer of the full data length.
    // Even if the value being persisted is smaller than this, we
    // write the full block so that the file always has an even
    // number of full blocks.  Otherwise, the (somewhat naive) scanning
    // algorithm will miss the last block.
    memset( buf + len, 0, sizeof(buf) - len );

    // copy data into buffer
    memcpy( buf, data, len );

    // write data
    // we write the entire buffer length so the entry size is always
    // consistent.
    fs_i16_write( f, buf, sizeof(buf) );

    kv_persist_writes++;

end:
    return 0;
}


static int8_t _kv_i8_persist_set(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    if( persist_fail ){

        return 0;
    }

    ASSERT( len <= KV_PERSIST_MAX_DATA_LEN );

    file_t f = fs_f_open_P( kv_data_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return -1;
    }

    int8_t status = _kv_i8_persist_set_internal( f, meta, hash, data, len );

    fs_f_close( f );

    return status;
}

static int8_t _kv_i8_internal_set(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    const void *data,
    uint16_t len )
{

    // check flags for readonly
    if( meta->flags & KV_FLAGS_READ_ONLY ){

        return KV_ERR_STATUS_READONLY;
    }

    uint16_t array_len = meta->array_len + 1;

    // bound index
    if( index >= array_len ){

        return KV_ERR_STATUS_OUT_OF_BOUNDS;
    }

    // set copy length
    uint16_t copy_len = type_u16_size( meta->type );

    // check if count and index are 0, if so, that requests the entire array/item
    if( ( index == 0 ) && ( count == 0 ) ){

        copy_len *= array_len;

        // set count to 1 so our loop works
        count = 1;
    }

    // do we have enough data?
    if( copy_len > len ){

        return KV_ERR_STATUS_TYPE_MISMATCH;
    }

    const void *ptr = data;

    // check if parameter has a pointer
    if( meta->ptr != 0 ){

        meta->ptr += ( index * type_u16_size( meta->type ) );

        int diff = 0;

        ATOMIC;

        for( uint16_t i = 0; i < count; i++ ){

            if( diff == 0 ){

                diff = memcmp( meta->ptr, ptr, copy_len );
            }

            // set data
            memcpy( meta->ptr, ptr, copy_len );

            meta->ptr += copy_len;
            ptr += copy_len;
            index++;

            // check for overflow
            if( index >= array_len ){

                break;
            }
        }   

        // check if dynamic and changed
        if( ( meta->flags & CATBUS_FLAGS_DYNAMIC ) && ( diff != 0 ) ){

            kvdb_i8_notify( hash );
        }

        END_ATOMIC;
    }

    int8_t status = KV_ERR_STATUS_OK;

    // check if array
    // we don't support the function mapping or persistence for arrays
    if( array_len == 1 ){

        // check if parameter has a notifier
        if( meta->handler != 0 ){

            // call handler
            status = meta->handler( KV_OP_SET, hash, (void *)data, copy_len );
        }

        // check if persist flag is set
        if( meta->flags & KV_FLAGS_PERSIST ){

            // check if we *don't* have a RAM pointer
            if( meta->ptr == 0 ){

                _kv_i8_persist_set( meta, hash, data, copy_len );
            }
            else{

                // signal thread to persist in background
                run_persist = TRUE;
            }
        }
    }

    return status;
}

int8_t kv_i8_set(
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    return kv_i8_array_set( hash, 0, 0, data, len );
}

int8_t kv_i8_array_set(
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    const void *data,
    uint16_t len )
{

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        return status;
    }

    return _kv_i8_internal_set( &meta, hash, index, count, data, len );
}


static int8_t _kv_i8_persist_get(
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{    
    file_t f = -1;

    if( kv_data_file_handle > 0 ){

        f = kv_data_file_handle;
    }
    else{

        f = fs_f_open_P( kv_data_fname, FS_MODE_READ_ONLY );
    }
    
    if( f < 0 ){

        return -1;
    }

    fs_v_seek( f, sizeof(kv_persist_file_header_t) );

    kv_persist_block_header_t hdr;
    memset( &hdr, 0, sizeof(hdr) ); // init to all 0s in case the file is empty

    // seek to matching item or end of file
    while( fs_i16_read( f, &hdr, sizeof(hdr) ) == sizeof(hdr) ){

        if( hdr.hash == hash ){

            break;
        }

        // advance position to next entry
        fs_v_seek( f, fs_i32_tell( f ) + KV_PERSIST_MAX_DATA_LEN );
    }

    uint16_t data_read = 0;

    // check if data was found
    if( hdr.hash == hash ){

        data_read = fs_i16_read( f, data, len );
    }

    // only close file if we opened it ourselves
    if( kv_data_file_handle < 0 ){
        
        fs_f_close( f );
    }

    // check if correct amount of data was read.
    // data_read will be 0 if the item was not found in the file.
    if( data_read != len ){

        return -1;
    }

    return 0;
}

int8_t kv_i8_internal_get(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t max_len )
{
    uint16_t array_len = meta->array_len + 1;

    // bound index
    if( index >= array_len ){

        return KV_ERR_STATUS_OUT_OF_BOUNDS;
    }

    // set copy length
    uint16_t copy_len = type_u16_size( meta->type );

    // check if count and index are 0, if so, that requests the entire array/item
    if( ( index == 0 ) && ( count == 0 ) ){

        copy_len *= array_len;

        // set count to 1 so our loop works
        count = 1;
    }

    // do we have enough data?
    if( copy_len > max_len ){

        return KV_ERR_STATUS_TYPE_MISMATCH;
    }

    // check if parameter has a pointer
    if( meta->ptr != 0 ){

        meta->ptr += ( index * type_u16_size( meta->type ) );

        // atomic because interrupts may access RAM data
        ATOMIC;

        for( uint16_t i = 0; i < count; i++ ){
            
            // get data
            memcpy( data, meta->ptr, copy_len );

            meta->ptr += copy_len;
            data += copy_len;
            index++;

            // check for overflow
            if( index >= array_len ){

                break;
            }
        }

        END_ATOMIC;
    }
    // didn't have a ram pointer:
    // check if persist flag is set, if it is,
    // we'll try to get data from the file.
    // also check if array:
    // we don't have persistence on arrays
    else if( ( meta->flags & KV_FLAGS_PERSIST ) && ( array_len == 1 ) ){

        // check data from file system
        if( _kv_i8_persist_get( hash, data, max_len ) < 0 ){

            // did not return any data, set default
            memset( data, 0, max_len );
        }
    }

    int8_t status = KV_ERR_STATUS_OK;

    // check if parameter has a notifier
    // AND is not an array
    if( ( meta->handler != 0 ) && ( array_len == 1 ) ){

        ATOMIC;

        // call handler
        status = meta->handler( KV_OP_GET, hash, data, copy_len );

        END_ATOMIC;    
    }

    return status;
}

int8_t kv_i8_get(
    catbus_hash_t32 hash,
    void *data,
    uint16_t max_len )
{

    return kv_i8_array_get( hash, 0, 0, data, max_len );
}

int8_t kv_i8_array_get(
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t max_len )
{

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        return status;
    }

    return kv_i8_internal_get( &meta, hash, index, count, data, max_len );
}

bool kv_b_get_boolean( catbus_hash_t32 hash ){

    bool val = FALSE;
    kv_i8_get( hash, &val, sizeof(val) );

    return val;
}

int16_t kv_i16_len( catbus_hash_t32 hash )
{
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        return status;
    }

    return kv_u16_get_size_meta( &meta );
}

catbus_type_t8 kv_i8_type( catbus_hash_t32 hash )
{

    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        return status;
    }

    return meta.type;
}

int8_t kv_i8_persist( catbus_hash_t32 hash )
{
    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        // parameter not found
        // log_v_error_P( PSTR("KV param not found") );

        return status;
    }

    // check for persist flag
    if( ( meta.flags & KV_FLAGS_PERSIST ) == 0 ){

        // log_v_error_P( PSTR("Persist flag not set!") );

        return -1;
    }

    // get parameter data
    uint8_t data[KV_PERSIST_MAX_DATA_LEN];
    kv_i8_internal_get( &meta, hash, 0, 1, data, sizeof(data) );

    // get parameter length
    uint16_t param_len = kv_u16_get_size_meta( &meta );

    return _kv_i8_persist_set( &meta, hash, data, param_len );
}



PT_THREAD( persist_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static kv_meta_t *ptr;
    static kv_meta_t *end_ptr;
    static file_t f;

    while(1){

        THREAD_WAIT_WHILE( pt, run_persist == FALSE );

        run_persist = FALSE;

        f = fs_f_open_P( kv_data_fname, FS_MODE_WRITE_OVERWRITE );

        if( f < 0 ){

            goto end;
        }

        kv_meta_t meta;
        ptr = (kv_meta_t *)kv_start;
        end_ptr = (kv_meta_t *)kv_end;

        if( sys_u8_get_mode() != SYS_MODE_SAFE ){

            end_ptr = (kv_meta_t *)kv_opt_end;
        }

        // iterate through handlers
        while( ptr < end_ptr ){

            // load meta data
            memcpy_P( &meta, ptr, sizeof(kv_meta_t) );

            // check flags - and that there is a RAM pointer
            if( ( ( meta.flags & KV_FLAGS_PERSIST ) != 0 ) &&
                  ( meta.ptr != 0 ) ){

                uint16_t param_len = kv_u16_get_size_meta( &meta );

                uint32_t hash = hash_u32_string( meta.name );
                _kv_i8_persist_set_internal( f, &meta, hash, meta.ptr, param_len );

                TMR_WAIT( pt, 5 );
            }   

            ptr++;
        }

        f = fs_f_close( f );

end:
        // prevent back to back updates from swamping the system
        TMR_WAIT( pt, 2000 );
    }

PT_END( pt );
}

uint8_t kv_u8_get_dynamic_count( void ){

    return kvdb_u16_count();
}

void kv_v_shutdown( void ){

    // run persistence on shutdown
    run_persist = TRUE;       
}

