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



#include <inttypes.h>
#include "bool.h"
#include "kvdb.h"
#include "memory.h"
#include "list.h"

#include "kvdb_config.h"

#ifdef KVDB_ENABLE_NAME_LOOKUP
#include "system.h"
#include "fs.h"
#include "hash.h"
#endif

// #include "logging.h"

static uint16_t kv_count;
static uint16_t db_size;
static mem_handle_t handle = -1;


typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    catbus_flags_t8 flags;
    uint8_t count;
    uint8_t tag;
    int32_t data;
} db_entry32_t;
// 8 bytes of meta data
// N bytes for data
// 5 bytes for mem block
// 2 bytes for list node
// in linked list: 15 bytes of overhead per item
// 19 bytes for i32
// 26 items in less than 512 bytes of RAM
// we will have to do linear searches for linked list. binary would require a tree or array.



#ifdef KVDB_ENABLE_NAME_LOOKUP
static const PROGMEM char kv_name_fname[] = "kv_names";

typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    char name[CATBUS_STRING_LEN];
} name_entry_t;
#endif

static int16_t cached_index = -1;
static catbus_hash_t32 cached_hash;

static int16_t _kvdb_i16_search_hash( catbus_hash_t32 hash ){

    if( hash == 0 ){

        return -1;
    }

    // check cache
    if( ( cached_index >= 0 ) && ( cached_hash == hash ) ){

        return cached_index;
    }

    db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );
    int16_t first = 0;
    int16_t last = db_size - 1;
    int16_t middle = ( first + last ) / 2;
    
    // binary search through hash index
    while( first <= last ){

        if( entry[middle].hash > hash ){

            first = middle + 1;
        }    
        else if( entry[middle].hash == hash ){

            cached_hash = hash;
            cached_index = middle;

            return middle;
        }
        else{

            last = middle - 1;
        }

        middle = ( first + last ) / 2;
    }

    return -1;
}

static void _kvdb_v_sort( void ){

    // bubble sort

    db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );

    bool swapped;
    do{

        swapped = FALSE;
    
        for( uint16_t i = 1; i < db_size; i++ ){            

            if( entry[i - 1].hash < entry[i].hash ){

                // swap values
                db_entry32_t temp = entry[i];
                entry[i] = entry[i - 1];
                entry[i - 1] = temp;

                swapped = TRUE;
            }
        }                

    } while( swapped );
}

#ifdef KVDB_ENABLE_NAME_LOOKUP
static void _kvdb_v_add_name( char name[CATBUS_STRING_LEN] ){

    file_t f = fs_f_open_P( kv_name_fname, FS_MODE_WRITE_APPEND | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return;
    }

    name_entry_t entry;
    entry.hash = hash_u32_string( name );
    memset( entry.name, 0, sizeof(entry.name) );
    strlcpy( entry.name, name, sizeof(entry.name) );

    fs_i16_write( f, (uint8_t *)&entry, sizeof(entry) );

    fs_f_close( f );
}
#endif

void kvdb_v_init( void ){

    #ifdef KVDB_ENABLE_SAFE_MODE_CHECK
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    #endif

    #ifdef KVDB_ENABLE_NAME_LOOKUP

    // delete name file
    file_t f = fs_f_open_P( kv_name_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f > 0 ){

        fs_v_delete( f );
            
        fs_f_close( f );
    }

    #endif

    // create initial database
    uint16_t size = KVDB_INITIAL_SIZE * sizeof(db_entry32_t);

    handle = mem2_h_alloc( size );

    if( handle < 0 ){

        return;
    }

    db_size = KVDB_INITIAL_SIZE;

    // init to all 0s
    uint8_t *ptr = mem2_vp_get_ptr( handle );

    memset( ptr, 0, size );


    // kvdb_i8_add( __KV__test_meow, 123, 0, "test_meow" );
    // kvdb_i8_add( __KV__test_woof, 456, 0, "test_woof" );
    // kvdb_i8_add( __KV__test_stuff, 999, 0, "test_stuff" );
    // kvdb_i8_add( __KV__test_things, 777, 0, "test_things" );
}

uint16_t kvdb_u16_count( void ){

    return kv_count;
}

uint16_t kvdb_u16_db_size( void ){

    return db_size * sizeof(db_entry32_t);
}

int8_t kvdb_i8_add( catbus_hash_t32 hash, int32_t data, uint8_t tag, char name[CATBUS_STRING_LEN] ){

    // try a set first
    int8_t status = kvdb_i8_set( hash, data );

    if( status == KVDB_STATUS_OK ){

        return status;
    }

    if( hash == 0 ){

        return KVDB_STATUS_INVALID_HASH;    
    }

    if( handle < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    // not found, we need to add this entry

    // check if we have enough space
    if( kv_count >= KVDB_MAX_ENTRIES ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    if( kv_count >= db_size ){

        uint16_t new_size = ( db_size + KVDB_SIZE_INCREMENT ) * sizeof(db_entry32_t);

        // try to increase database size
        if( mem2_i8_realloc( handle, new_size ) < 0 ){

            return KVDB_STATUS_NOT_ENOUGH_SPACE;    
        }

        // success!

        // lets 0 out the new entries
        db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );

        for( uint16_t i = db_size; i < ( db_size + KVDB_SIZE_INCREMENT ); i++ ){

            entry[i].hash = 0;
        }

        // adjust db size
        db_size += KVDB_SIZE_INCREMENT;
    }

    db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );

    // find a free index
    for( uint16_t i = 0; i < db_size; i++ ){

        // check if free
        if( entry[i].hash == 0 ){

            // add
            entry[i].hash   = hash;
            entry[i].data   = data;
            entry[i].flags  = CATBUS_FLAGS_DYNAMIC;
            entry[i].tag    = tag;
            entry[i].type   = CATBUS_TYPE_INT32;

            #ifdef KVDB_ENABLE_NAME_LOOKUP
            // add name
            if( name != 0 ){

                _kvdb_v_add_name( name );
            }
            #endif

            break;
        }
    }

    kv_count++;

    // now we need to sort

    // bubble sort
    _kvdb_v_sort();


    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_set( catbus_hash_t32 hash, int32_t data ){

    if( hash == 0 ){

        return KVDB_STATUS_INVALID_HASH;    
    }

    if( handle < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }
    
    // get index for hash
    int16_t index = _kvdb_i16_search_hash( hash );

    // check if found
    if( index >= 0 ){

        db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );

        bool changed = entry[index].data != data;

        entry[index].data = data;

        // check if there is a notifier and data is changing
        if( ( kvdb_v_notify_set != 0 ) && ( changed ) ){

            catbus_meta_t meta;
            kvdb_i8_get_meta( hash, &meta );

            kvdb_v_notify_set( hash, &meta, &data );
        }

        return KVDB_STATUS_OK;
    }
    
    return KVDB_STATUS_NOT_FOUND;    
}

int8_t kvdb_i8_get( catbus_hash_t32 hash, int32_t *data ){

    if( handle < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }
    
    // get index for hash
    int16_t index = _kvdb_i16_search_hash( hash );

    // check if found
    if( index >= 0 ){

        db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );
        *data = entry[index].data;

        return KVDB_STATUS_OK;
    }
    
    // not found
    // set data to 0 so we at least have a sane default
    *data = 0;

    return KVDB_STATUS_NOT_FOUND;
}

int8_t kvdb_i8_get_meta( catbus_hash_t32 hash, catbus_meta_t *meta ){

    if( handle < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }
    
    // get index for hash
    int16_t index = _kvdb_i16_search_hash( hash );

    // check if found
    if( index >= 0 ){

        db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );
        
        meta->hash      = hash;
        meta->count     = 0;
        meta->flags     = entry[index].flags;
        meta->type      = entry[index].type;
        meta->reserved  = 0;

        return KVDB_STATUS_OK;
    }
    
    // not found
    // set data to 0 so we at least have a sane default
    memset( meta, 0, sizeof(catbus_meta_t) );

    return KVDB_STATUS_NOT_FOUND;
}


int8_t kvdb_i8_delete( catbus_hash_t32 hash ){

    if( handle < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    // get index for hash
    int16_t index = _kvdb_i16_search_hash( hash );

    if( index >= 0 ){

        db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );
        entry[index].hash = 0;

        kv_count--;

        // need to resort

        _kvdb_v_sort();

        // reset cache
        cached_index = -1;

        return KVDB_STATUS_OK;
    }
    
    return KVDB_STATUS_NOT_FOUND;    
}

void kvdb_v_delete_tag( uint8_t tag ){

    if( handle < 0 ){

        return;
    }

    db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );

    for( uint16_t i = 0; i < db_size; i++ ){

        if( entry[i].hash == 0 ){

            continue;
        }

        if( entry[i].tag == tag ){

            entry[i].hash = 0;       
            kv_count--;
        }
    }

    // reset cache
    cached_index = -1;

    _kvdb_v_sort();
}

int8_t kvdb_i8_publish( catbus_hash_t32 hash ){

    if( kvdb_i8_handle_publish == 0 ){

        return KVDB_STATUS_OK;
    }

    return kvdb_i8_handle_publish( hash );
}

#ifdef KVDB_ENABLE_NAME_LOOKUP
int8_t kvdb_i8_lookup_name( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN] ){

    int8_t status = KVDB_STATUS_NOT_FOUND;

    file_t f = fs_f_open_P( kv_name_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return status;
    }

    name_entry_t entry;

    while( fs_i16_read( f, (uint8_t *)&entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.hash == hash ){

            strlcpy( name, entry.name, CATBUS_STRING_LEN );

            status = KVDB_STATUS_OK;

            break;
        }
    }

    fs_f_close( f );

    return status;
}
#endif

catbus_hash_t32 kvdb_h_get_hash_for_index( uint16_t index ){

    if( handle < 0 ){

        return 0;
    }

    if( index >= db_size ){

        return 0;
    }

    db_entry32_t *entry = (db_entry32_t *)mem2_vp_get_ptr( handle );

    return entry[index].hash;
}

int16_t kvdb_i16_get_index_for_hash( catbus_hash_t32 hash ){

    if( handle < 0 ){

        return 0;
    }

    return _kvdb_i16_search_hash( hash );
}

// direct retrieval functions, for those who like to throw caution to the wind!
uint16_t kvdb_u16_read( catbus_hash_t32 hash ){

    int32_t data;
    kvdb_i8_get( hash, &data );

    return data;
}

uint8_t kvdb_u8_read( catbus_hash_t32 hash ){

    int32_t data;
    kvdb_i8_get( hash, &data );

    return data;
}

int8_t kvdb_i8_read( catbus_hash_t32 hash ){

    int32_t data;
    kvdb_i8_get( hash, &data );

    return data;
}

int16_t kvdb_i16_read( catbus_hash_t32 hash ){

    int32_t data;
    kvdb_i8_get( hash, &data );

    return data;
}

int32_t kvdb_i32_read( catbus_hash_t32 hash ){

    int32_t data;
    kvdb_i8_get( hash, &data );

    return data;
}

bool kvdb_b_read( catbus_hash_t32 hash ){

    int32_t data;
    kvdb_i8_get( hash, &data );

    return data;
}