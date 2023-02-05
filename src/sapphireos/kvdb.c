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



#include <inttypes.h>
#include "bool.h"
#include "kvdb.h"
#include "memory.h"
#include "list.h"
#include "keyvalue.h"

#include "kvdb_config.h"

#ifdef KVDB_ENABLE_NAME_LOOKUP
#include "system.h"
#include "fs.h"
#include "hash.h"
#endif

#include "datalogger.h"
#include "logging.h"


static list_t db_list;
static catbus_hash_t32 cache_hash;
static list_node_t cache_ln;

#ifdef KVDB_ENABLE_NAME_LOOKUP
static file_t kv_names_f = -1;
#endif


typedef struct{
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    catbus_flags_t8 flags;
    uint8_t count;
    uint8_t tag;
    #ifdef KVDB_ENABLE_NOTIFIER
    kvdb_notifier_t notifier;
    #endif
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

    list_node_t ln;

    if( cache_hash == hash ){

        ln = cache_ln;
    }
    else{

        ln = db_list.head;
    }

    while( ln > 0 ){

        db_entry_t *entry = list_vp_get_data( ln );

        if( entry->hash == hash ){

            cache_hash = hash;
            cache_ln = ln;

            return entry;
        }

        ln = list_ln_next( ln );
    }

    return 0;
}

static void _kvdb_v_reset_cache( void ){

    cache_hash = 0;
    cache_ln = -1;    
}


#ifdef KVDB_ENABLE_NAME_LOOKUP
static void _kvdb_v_add_name( char name[CATBUS_STRING_LEN] ){

    uint32_t hash = hash_u32_string( name );

    // check if we have this name already
    if( kvdb_i8_lookup_name( hash, 0 ) == KVDB_STATUS_OK ){

        return;
    }

    if( kv_names_f < 0 ){

        return;
    }

    name_entry_t entry;
    entry.hash = hash;
    memset( entry.name, 0, sizeof(entry.name) );
    strlcpy( entry.name, name, sizeof(entry.name) );

    fs_i16_write( kv_names_f, (uint8_t *)&entry, sizeof(entry) );
}
#endif

void kvdb_v_init( void ){

    #ifdef KVDB_ENABLE_NAME_LOOKUP

    // don't delete name file, this would break hash lookups on manually installed links

    // // delete name file
    // file_t f = fs_f_open_P( kv_name_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    // if( f > 0 ){

    //     fs_v_delete( f );
            
    //     fs_f_close( f );
    // }
    
    kv_names_f = fs_f_open_P( kv_name_fname, FS_MODE_WRITE_APPEND | FS_MODE_CREATE_IF_NOT_FOUND );

    #endif

    list_v_init( &db_list );
}

uint16_t kvdb_u16_count( void ){

    return list_u8_count( &db_list );
}

uint16_t kvdb_u16_db_size( void ){

    return list_u16_size( &db_list );
}


// TODO:
// require adding the name to the DB, otherwise lookups will fail!
int8_t kvdb_i8_add( 
    catbus_hash_t32 hash, 
    catbus_type_t8 type,
    uint16_t count,
    const void *data,
    uint16_t len ){

    // try a set first
    if( data != 0 ){

        int8_t status = kvdb_i8_set( hash, type, data, len );

        if( status == KVDB_STATUS_OK ){

            return status;
        }
    }

    if( hash == 0 ){

        return KVDB_STATUS_INVALID_HASH;    
    }

    // check if this hash is already in another part of the database:
    catbus_meta_t meta;
    int8_t status = kv_i8_get_catbus_meta( hash, &meta );
    if( status >= 0 ){

        return KVDB_STATUS_HASH_CONFLICT;
    }

    if( count > 256 ){

        count = 256;
    }

    if( count == 0 ){

        count = 1;
    }

    uint16_t data_len = type_u16_size( type ) * count;

    if( data_len > KVDB_MAX_DATA ){

        return KVDB_STATUS_DATA_TOO_LARGE;
    }

    count--;

    // not found, we need to add this entry
    #ifdef MEM_TYPE_KVDB_ENTRY
    list_node_t ln = list_ln_create_node2( 0, sizeof(db_entry_t) + data_len, MEM_TYPE_KVDB_ENTRY );
    #else
    list_node_t ln = list_ln_create_node( 0, sizeof(db_entry_t) + data_len );
    #endif

    if( ln < 0 ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    // reset cache!
    kv_v_reset_cache();

    db_entry_t *entry = list_vp_get_data( ln );

    entry->hash      = hash;
    entry->type      = type;
    entry->flags     = CATBUS_FLAGS_DYNAMIC;
    entry->tag       = 0;
    entry->count     = count;
    #ifdef KVDB_ENABLE_NOTIFIER
    entry->notifier  = 0;
    #endif

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );

    if( data != 0 ){
        
        memcpy( data_ptr, data, data_len );
    }
    else{

        memset( data_ptr, 0, data_len );
    }

    list_v_insert_tail( &db_list, ln );

    // notify on adding data
    if( kvdb_v_notify_set != 0 ){

        catbus_meta_t meta;
        kvdb_i8_get_meta( hash, &meta );

        kvdb_v_notify_set( hash, &meta, data_ptr );
    }

    // refresh the datalogger config so it is aware 
    // of the new keys we've added.
    datalogger_v_refresh_config();

    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_set_persist( catbus_hash_t32 hash, bool persist ){

    if( hash == 0 ){

        return KVDB_STATUS_INVALID_HASH;    
    }

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){

        return KVDB_STATUS_NOT_FOUND;    
    }

    if( persist ){

        entry->flags |= KV_FLAGS_PERSIST;
    }
    else{

        entry->flags &= ~KV_FLAGS_PERSIST;   
    }

    return KVDB_STATUS_OK;
}

void kvdb_v_set_name( char name[CATBUS_STRING_LEN] ){
    
    #ifdef KVDB_ENABLE_NAME_LOOKUP
    _kvdb_v_add_name( name );   
    #endif 
}


#ifdef PGM_P
void kvdb_v_set_name_P( PGM_P name ){

    char buf[CATBUS_STRING_LEN];
    strlcpy_P( buf, name, sizeof(buf) );

    #ifdef KVDB_ENABLE_NAME_LOOKUP
    _kvdb_v_add_name( buf );   
    #endif 
}
#endif

void kvdb_v_set_tag( catbus_hash_t32 hash, uint8_t tag ){

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){

        return;
    }

    entry->tag |= tag;
}

void kvdb_v_clear_tag( catbus_hash_t32 hash, uint8_t tag ){

    if( hash == 0 ){

        // clear all entries matching tag
        list_node_t ln = db_list.head;

        while( ln > 0 ){

            list_node_t next_ln = list_ln_next( ln );

            db_entry_t *entry = list_vp_get_data( ln );

            if( entry->tag & tag ){

                entry->tag &= ~tag;
            
                if( entry->tag == 0 ){

                    // purge entry
                    list_v_remove( &db_list, ln );
                    list_v_release_node( ln );
                }
            }

            ln = next_ln;
        }

        _kvdb_v_reset_cache();
        kv_v_reset_cache();
    }
    else{

        // get entry for hash
        db_entry_t *entry = _kvdb_dbp_search_hash( hash );

        if( entry == 0 ){

            return;
        }

        entry->tag &= ~tag;

        // check if tag is 0, if so, purge
        if( entry->tag == 0 ){

            kvdb_v_delete( hash );        
        }
    }
}

void kvdb_v_set_notifier( catbus_hash_t32 hash, kvdb_notifier_t notifier ){

    #ifdef KVDB_ENABLE_NOTIFIER
    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){

        return;
    }

    entry->notifier = notifier;
    #endif
}

int8_t kvdb_i8_set( catbus_hash_t32 hash, catbus_type_t8 type, const void *data, uint16_t len ){

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
    uint16_t type_size = type_u16_size( type );
    uint16_t data_len = type_size * ( (uint16_t)entry->count + 1 );

    if( type_b_is_string( type ) ){

        // string types must be truncated to destination length
        if( len > type_size ){

            len = type_size;
        }
    }
    // scalar types must have a matching data length
    else if( len != data_len ){

        return KVDB_STATUS_LENGTH_MISMATCH;
    }

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );
    bool changed = FALSE;

    uint16_t count = entry->count + 1;

    while( count > 0 ){

        int8_t convert = type_i8_convert( entry->type, data_ptr, type, data, len );

        if( convert != 0 ){

            changed = TRUE;
        }

        data = (uint8_t *)data + type_size;
        data_ptr = (uint8_t *)data_ptr + type_size;

        count--;
    }
    
    data_ptr = (uint8_t *)( entry + 1 );

    // check if data is changing
    if( changed ){

        // check if persist is set:
        if( entry->flags & KV_FLAGS_PERSIST ){

            kv_i8_persist( entry->hash );
        }        

        // check if there is a notifier
        if( kvdb_v_notify_set != 0 ){

            catbus_meta_t meta;
            kvdb_i8_get_meta( hash, &meta );

            kvdb_v_notify_set( hash, &meta, data_ptr );
        }

        #ifdef KVDB_ENABLE_NOTIFIER
        if( entry->notifier != 0 ){

            entry->notifier( hash, entry->type, data_ptr );
        }
        #endif
    }

    return KVDB_STATUS_OK;
}


int8_t kvdb_i8_get( catbus_hash_t32 hash, catbus_type_t8 type, void *data, uint16_t max_len ){

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){
        
        return KVDB_STATUS_NOT_FOUND;
    }

    uint16_t data_len = type_u16_size(type) * ( entry->count + 1 );

    if( max_len < data_len ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    // send type NONE indicates that source and destination types match
    if( type == CATBUS_TYPE_NONE ){

        type = entry->type;
    }

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );

    for( uint8_t i = 0; i <= entry->count; i++ ){
        
        type_i8_convert( type, data, entry->type, data_ptr, 0 );

        data = (uint8_t *)data + type_u16_size( type );
        data_ptr = (uint8_t *)data_ptr + type_u16_size( entry->type );
    }

    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_array_set( catbus_hash_t32 hash, catbus_type_t8 type, uint16_t index, const void *data, uint16_t len ){

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

    uint16_t type_len = type_u16_size( type );
    uint16_t array_len = entry->count + 1;

    if( len > type_len ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    // wrap around index
    if( index >= array_len ){
        
        index %= array_len;
    }

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );
    bool changed = FALSE;

    int8_t convert = type_i8_convert( entry->type, data_ptr + ( index * type_u16_size( entry->type ) ), type, data, len );

    if( convert != 0 ){

        changed = TRUE;
    }

    // check if there is a notifier and data is changing
    if( changed ){

        if( kvdb_v_notify_set != 0 ){

            catbus_meta_t meta;
            kvdb_i8_get_meta( hash, &meta );

            kvdb_v_notify_set( hash, &meta, data_ptr );
        }

        #ifdef KVDB_ENABLE_NOTIFIER
        if( entry->notifier != 0 ){

            entry->notifier( hash, entry->type, data_ptr );
        }
        #endif
    }

    return KVDB_STATUS_OK;
}


// get a single item from an array
int8_t kvdb_i8_array_get( catbus_hash_t32 hash, catbus_type_t8 type, uint16_t index, void *data, uint16_t max_len ){

    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){
        
        return KVDB_STATUS_NOT_FOUND;
    }

    // send type NONE indicates that source and destination types match
    if( type == CATBUS_TYPE_NONE ){

        type = entry->type;
    }

    uint16_t type_len = type_u16_size( type );
    uint16_t array_len = entry->count + 1;

    if( max_len < type_len ){

        return KVDB_STATUS_NOT_ENOUGH_SPACE;
    }

    // wrap around index
    if( index >= array_len ){
        
        index %= array_len;
    }

    uint8_t *data_ptr = (uint8_t *)( entry + 1 );
        
    type_i8_convert( type, data, entry->type, data_ptr + ( index * type_u16_size( entry->type ) ), 0 );

    return KVDB_STATUS_OK;
}

int8_t kvdb_i8_notify( catbus_hash_t32 hash ){

    #ifdef KVDB_ENABLE_NOTIFIER
    // get entry for hash
    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){
        
        return -1;
    }    

    if( entry->notifier != 0 ){

        entry->notifier( hash, entry->type, (void *)( entry + 1 ) );
    }

    #endif

    return 0;
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
    meta->count     = entry->count;
    meta->flags     = entry->flags;
    meta->type      = entry->type;
    meta->reserved  = 0;

    return KVDB_STATUS_OK;
}

void *kvdb_vp_get_ptr( catbus_hash_t32 hash ){

    db_entry_t *entry = _kvdb_dbp_search_hash( hash );

    if( entry == 0 ){

        return 0;
    }

    return entry + 1;
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

    _kvdb_v_reset_cache();

    kv_v_reset_cache();
}

#ifdef KVDB_ENABLE_NAME_LOOKUP
int8_t kvdb_i8_lookup_name( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN] ){

    int8_t status = KVDB_STATUS_NOT_FOUND;

    if( kv_names_f < 0 ){

        goto end;
    }

    fs_v_seek( kv_names_f, 0 );

    name_entry_t entry;

    while( fs_i16_read( kv_names_f, (uint8_t *)&entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.hash == hash ){

            if( name != 0 ){
                
                strlcpy( name, entry.name, CATBUS_STRING_LEN );
            }

            status = KVDB_STATUS_OK;

            break;
        }
    }

end:
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

