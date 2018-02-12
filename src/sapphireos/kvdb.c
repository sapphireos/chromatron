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

#ifndef ESP8266
#include "logging.h"
#endif


static list_t db_list;


typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    catbus_flags_t8 flags;
    uint8_t count;
    uint8_t tag;
} db_entry_t;
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

static db_entry_t * _kvdb_dbp_search_hash( catbus_hash_t32 hash ){

    if( hash == 0 ){

        return 0;
    }

    list_node_t ln = db_list.head;

    while( ln > 0 ){

        db_entry_t *entry = list_vp_get_data( ln );

        if( entry->hash == hash ){

            return entry;
        }

        ln = list_ln_next( ln );
    }   

    return 0;
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

    list_v_init( &db_list );

    int32_t temp = 123;
    kvdb_i8_add( __KV__test_meow, CATBUS_TYPE_INT32, &temp, 0, "test_meow" );    

    // kvdb_i8_add( __KV__test_meow, 123, 0, "test_meow" );
    // kvdb_i8_add( __KV__test_woof, 456, 0, "test_woof" );
    // kvdb_i8_add( __KV__test_stuff, 999, 0, "test_stuff" );
    // kvdb_i8_add( __KV__test_things, 777, 0, "test_things" );
}

uint16_t kvdb_u16_count( void ){

    return list_u8_count( &db_list );
}

uint16_t kvdb_u16_db_size( void ){

    return list_u16_size( &db_list );
}

int8_t kvdb_i8_add( 
    catbus_hash_t32 hash, 
    catbus_type_t8 type,
    const void *data,
    uint8_t tag, 
    char name[CATBUS_STRING_LEN] ){

    // try a set first
    if( data != 0 ){
        
        int8_t status = kvdb_i8_set( hash, type, data );

        if( status == KVDB_STATUS_OK ){

            return status;
        }
    }

    if( hash == 0 ){

        return KVDB_STATUS_INVALID_HASH;    
    }

    // not found, we need to add this entry
    #ifdef MEM_TYPE_KVDB_ENTRY
    list_node_t ln = list_ln_create_node2( 0, sizeof(db_entry_t) + type_u16_size(type), MEM_TYPE_KVDB_ENTRY );
    #else
    list_node_t ln = list_ln_create_node( 0, sizeof(db_entry_t) + type_u16_size(type) );
    #endif

    if( ln < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    db_entry_t *entry = list_vp_get_data( ln );

    entry->hash      = hash;
    entry->type      = type;
    entry->flags     = CATBUS_FLAGS_DYNAMIC;
    entry->tag       = tag;
    entry->count     = 1;

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );

    if( data != 0 ){
        
        memcpy( data_ptr, data, type_u16_size(type) );
    }
    else{

        memset( data_ptr, 0, type_u16_size(type) );
    }

    list_v_insert_tail( &db_list, ln );

    #ifdef KVDB_ENABLE_NAME_LOOKUP
    // add name
    if( name != 0 ){

        _kvdb_v_add_name( name );
    }
    #endif

    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_set( catbus_hash_t32 hash, catbus_type_t8 type, const void *data ){

    if( hash == 0 ){

        return KVDB_STATUS_INVALID_HASH;    
    }

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){

        return KVDB_STATUS_NOT_FOUND;    
    }

    // send type NONE indicates that source and destination types match
    if( type == CATBUS_TYPE_NONE ){

        type = entry->type;
    }

    // log_v_debug_P(PSTR("DB set: %lx type: %d"), hash, (uint16_t)type);

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );
    int8_t convert = type_i8_convert( entry->type, data_ptr, type, data );
    bool changed = ( convert != 0 );

    // log_v_debug_P(PSTR("SET: %lx = %ld src: %ld"), hash, *(int32_t*)data_ptr, *(int32_t*)data);

    // check if there is a notifier and data is changing
    if( ( kvdb_v_notify_set != 0 ) && ( changed ) ){

        catbus_meta_t meta;
        kvdb_i8_get_meta( hash, &meta );

        kvdb_v_notify_set( hash, &meta, data );
    }

    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_get( catbus_hash_t32 hash, catbus_type_t8 type, void *data ){

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){
        
        return KVDB_STATUS_NOT_FOUND;
    }

    // send type NONE indicates that source and destination types match
    if( type == CATBUS_TYPE_NONE ){

        type = entry->type;
    }

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );
    type_i8_convert( type, data, entry->type, data_ptr );

    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_get_meta( catbus_hash_t32 hash, catbus_meta_t *meta ){

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){


        // not found
        // set data to 0 so we at least have a sane default
        memset( meta, 0, sizeof(catbus_meta_t) );

        return KVDB_STATUS_NOT_FOUND;    
    }

    meta->hash      = hash;
    meta->count     = 0;
    meta->flags     = entry->flags;
    meta->type      = entry->type;
    meta->reserved  = 0;

    return KVDB_STATUS_OK;
}


void kvdb_v_delete( catbus_hash_t32 hash ){

    list_node_t ln = db_list.head;

    while( ln > 0 ){

        db_entry_t *entry = list_vp_get_data( ln );

        if( entry->hash == hash ){

            list_v_remove( &db_list, ln );
            list_v_release_node( ln );
            break;
        }

        ln = list_ln_next( ln );
    }   
}

void kvdb_v_delete_tag( uint8_t tag ){

    list_node_t ln = db_list.head;
    list_node_t next_ln = -1;

    while( ln > 0 ){

        next_ln = list_ln_next( ln );

        db_entry_t *entry = list_vp_get_data( ln );

        if( entry->tag == tag ){

            list_v_remove( &db_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }   
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

    uint16_t i = 0;

    list_node_t ln = db_list.head;

    while( ln > 0 ){

        if( i == index ){

            db_entry_t *entry = list_vp_get_data( ln );

            return entry->hash;
        }

        ln = list_ln_next( ln );
        i++;
    }   

    return 0;
}

int16_t kvdb_i16_get_index_for_hash( catbus_hash_t32 hash ){

    uint16_t i = 0;

    list_node_t ln = db_list.head;

    while( ln > 0 ){

        db_entry_t *entry = list_vp_get_data( ln );

        if( entry->hash == hash ){

            return i;

            return entry->hash;
        }

        ln = list_ln_next( ln );
        i++;
    }   

    return -1;
}


// // direct retrieval functions, for those who like to throw caution to the wind!
// uint16_t kvdb_u16_read( catbus_hash_t32 hash ){

//     int32_t data;
//     kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data );

//     return data;
// }

// uint8_t kvdb_u8_read( catbus_hash_t32 hash ){

//     int32_t data;
//     kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data );

//     return data;
// }

// int8_t kvdb_i8_read( catbus_hash_t32 hash ){

//     int32_t data;
//     kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data );

//     return data;
// }

// int16_t kvdb_i16_read( catbus_hash_t32 hash ){

//     int32_t data;
//     kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data );

//     return data;
// }

// int32_t kvdb_i32_read( catbus_hash_t32 hash ){

//     int32_t data;
//     kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data );

//     return data;
// }

// bool kvdb_b_read( catbus_hash_t32 hash ){

//     int32_t data;
//     kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data );

//     return data;
// }