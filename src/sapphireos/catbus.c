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

#include "sapphire.h"

#include "config.h"
#include "hash.h"
#include "catbus.h"
#include "random.h"
#include "timesync.h"
#include "services.h"

// #define NO_LOGGING
#include "logging.h"


#ifdef ENABLE_NETWORK
static uint64_t origin_id;

static socket_t sock;
static thread_t file_sessions[CATBUS_MAX_FILE_SESSIONS];

static list_t name_lookup_list;
#endif

static catbus_hash_t32 meta_tag_hashes[CATBUS_QUERY_LEN];
// start of adjustable tags through the add/rm interface
// tags before this index must be set directly
#define CATBUS_META_TAGS_START 2 


PT_THREAD( catbus_server_thread( pt_t *pt, void *state ) );
PT_THREAD( catbus_announce_thread( pt_t *pt, void *state ) );
PT_THREAD( catbus_shutdown_thread( pt_t *pt, void *state ) );


static int8_t _catbus_i8_meta_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len );

KV_SECTION_META kv_meta_t catbus_metatags_kv[] = {
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_name" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_location" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_0" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_1" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_2" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_3" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_4" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_5" },

    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler, "meta_cmd_add" },
    { CATBUS_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler, "meta_cmd_rm" },
};

static const PROGMEM catbus_hash_t32 meta_tag_names[CATBUS_QUERY_LEN] = {
    CFG_PARAM_META_TAG_0,
    CFG_PARAM_META_TAG_1,
    CFG_PARAM_META_TAG_2,
    CFG_PARAM_META_TAG_3,
    CFG_PARAM_META_TAG_4,
    CFG_PARAM_META_TAG_5,
    CFG_PARAM_META_TAG_6,
    CFG_PARAM_META_TAG_7,
};

static void _catbus_v_setup_tag_hashes( void ){
    
    for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

        char s[CATBUS_STRING_LEN];
        memset( s, 0, sizeof(s) );

        // read tag name hash from flash lookup
        catbus_hash_t32 hash;
        memcpy_P( &hash, &meta_tag_names[i], sizeof(hash) );

        if( cfg_i8_get( hash, s ) == 0 ){

            meta_tag_hashes[i] = hash_u32_string( s );

            // add to names db
            kvdb_v_set_name( s );
        }
        else{

            meta_tag_hashes[i] = 0;   
        }
    }
}

static void _catbus_v_add_tag( char *s ){

    char tag[CATBUS_STRING_LEN];
    memset( tag, 0, sizeof(tag) );
    strlcpy( tag, s, sizeof(tag) );

    uint32_t hash = hash_u32_string( tag );

    // first check if we already have this tag
    for( uint8_t i = CATBUS_META_TAGS_START; i < CATBUS_QUERY_LEN; i++ ){
        
        if( meta_tag_hashes[i] == hash ){

            return;
        }
    }

    // look for free entry
    for( uint8_t i = CATBUS_META_TAGS_START; i < CATBUS_QUERY_LEN; i++ ){

        if( meta_tag_hashes[i] == 0 ){

            // read tag name hash from flash lookup
            catbus_hash_t32 tag_hash;
            memcpy_P( &tag_hash, &meta_tag_names[i], sizeof(tag_hash) );

            cfg_v_set( tag_hash, tag );

            break;
        }
    }

    _catbus_v_setup_tag_hashes();
}

static void _catbus_v_rm_tag( char *s ){

    char tag[CATBUS_STRING_LEN];
    memset( tag, 0, sizeof(tag) );
    strlcpy( tag, s, sizeof(tag) );

    uint32_t hash = hash_u32_string( tag );

    for( uint8_t i = CATBUS_META_TAGS_START; i < CATBUS_QUERY_LEN; i++ ){

        if( meta_tag_hashes[i] == hash ){

            // read tag name hash from flash lookup
            catbus_hash_t32 tag_hash;
            memcpy_P( &tag_hash, &meta_tag_names[i], sizeof(tag_hash) );

            cfg_v_erase( tag_hash );
        }
    }

    _catbus_v_setup_tag_hashes();
}


static int8_t _catbus_i8_meta_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        if( hash == __KV__meta_cmd_add ){
            
            _catbus_v_add_tag( data );
        }
        else if( hash == __KV__meta_cmd_rm ){

            _catbus_v_rm_tag( data );   
        }
        else{

            int8_t status = cfg_i8_kv_handler( op, hash, data, len );    

            _catbus_v_setup_tag_hashes();

            return status;
        }
    }
    else if( op == KV_OP_GET ){

        if( ( hash == __KV__meta_cmd_add ) ||
            ( hash == __KV__meta_cmd_rm ) ){

            memset( data, 0, len );
        }
        else{

            return cfg_i8_kv_handler( op, hash, data, len );     
        }
    }
    
    return 0;
}



void catbus_v_init( void ){

    COMPILER_ASSERT( CATBUS_FILE_PAGE_SIZE <= CATBUS_MAX_DATA );

    trace_printf("Catbus max data len: %d\r\n", CATBUS_MAX_DATA);

    _catbus_v_setup_tag_hashes();

    #ifdef ENABLE_CATBUS_LINK
    // list_v_init( &send_list );
    // list_v_init( &receive_cache );
    
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

    }
    else{
        
        link_v_init();
    }
    #endif

    #ifdef ENABLE_NETWORK

    list_v_init( &name_lookup_list );

    thread_t_create( catbus_server_thread,
                     PSTR("catbus_server"),
                     0,
                     0 );

    #ifdef ENABLE_WIFI
    thread_t_create( catbus_announce_thread,
                     PSTR("catbus_announce"),
                     0,
                     0 );
    #endif
    #endif
}

void catbus_v_set_options( uint32_t options ){

}

#ifdef ENABLE_NETWORK
static void _catbus_v_msg_init( catbus_header_t *header, 
                                uint8_t msg_type,
                                uint32_t transaction_id ){

    if( transaction_id == 0 ){

        transaction_id = ( (uint32_t)rnd_u16_get_int() << 16 ) + rnd_u16_get_int();
    }

    header->meow            = CATBUS_MEOW;
    header->version         = CATBUS_VERSION;
    header->flags           = 0;
    header->reserved        = 0;
    header->msg_type        = msg_type;
    header->transaction_id  = transaction_id;

    header->universe        = 0;

    header->origin_id       = origin_id;
}

void catbus_v_get_query( catbus_query_t *query ){

    for( uint8_t i = 0; i < cnt_of_array(query->tags); i++ ){

        query->tags[i] = meta_tag_hashes[i];
    }
}

static bool _catbus_b_has_tag( catbus_hash_t32 hash ){

    for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

        if( meta_tag_hashes[i] == hash ){

            return TRUE;
        }
    }

    return FALSE;
}

bool catbus_b_query_self( catbus_query_t *query ){

    for( uint8_t i = 0; i < cnt_of_array(query->tags); i++ ){

        if( query->tags[i] == 0 ){        

            continue;
        }

        if( !_catbus_b_has_tag( query->tags[i] ) ){

            return FALSE;
        }
    }   

    return TRUE;
}

// return TRUE if has is found in tags
bool catbus_b_query_single( catbus_hash_t32 hash, catbus_query_t *tags ){

    if( hash == 0 ){

        return TRUE;
    }

    for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

        if( hash == tags->tags[i] ){

            return TRUE;
        }
    }

    return FALSE;
}

// return TRUE if all hashes in the query are found in the tags
bool catbus_b_query_tags( catbus_query_t *query, catbus_query_t *tags ){

    for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

        if( !catbus_b_query_single( query->tags[i], tags ) ){

            return FALSE;
        }
    }

    return TRUE;
}


static void _catbus_v_send_announce( sock_addr_t *raddr, uint32_t discovery_id ){

    if( sys_b_is_shutting_down() ){

        return;
    }

    mem_handle_t h = mem2_h_alloc( sizeof(catbus_msg_announce_t) );

    if( h < 0 ){

        return;
    }

    catbus_msg_announce_t *msg = mem2_vp_get_ptr( h );
    _catbus_v_msg_init( &msg->header, CATBUS_MSG_TYPE_ANNOUNCE, discovery_id );

    msg->flags = 0;
    msg->data_port = sock_u16_get_lport( sock );

    catbus_v_get_query( &msg->query );

    sock_i16_sendto_m( sock, h, raddr );
}

static void _catbus_v_broadcast_announce( void ){

    sock_addr_t raddr;
    raddr.ipaddr = ip_a_addr(CATBUS_ANNOUNCE_MCAST_ADDR);
    raddr.port = CATBUS_ANNOUNCE_PORT;

    _catbus_v_send_announce( &raddr, 0 );
}

static void _catbus_v_transmit_shutdown( sock_addr_t *raddr ){

    mem_handle_t h = mem2_h_alloc( sizeof(catbus_msg_shutdown_t) );

    if( h < 0 ){

        return;
    }

    catbus_msg_shutdown_t *msg = mem2_vp_get_ptr( h );
    _catbus_v_msg_init( &msg->header, CATBUS_MSG_TYPE_SHUTDOWN, 0 );

    msg->flags = 0;
            
    sock_i16_sendto_m( sock, h, raddr );
}

static void _catbus_v_send_shutdown( void ){
    
    sock_addr_t raddr;
    
    // broadcast shutdown to main port
    raddr.port = CATBUS_MAIN_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);
    _catbus_v_transmit_shutdown( &raddr );

    // multicast shutdown to announce port
    raddr.port = CATBUS_ANNOUNCE_PORT;
    raddr.ipaddr = ip_a_addr(CATBUS_ANNOUNCE_MCAST_ADDR);
    _catbus_v_transmit_shutdown( &raddr );
}
#endif


int8_t _catbus_i8_internal_set(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t data_len ){

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        return status;
    }

    uint16_t array_len = meta.array_len + 1;

    // wrap index
    if( index > array_len ){

        index %= array_len;
    }

    bool changed = FALSE;

    uint8_t buf[CATBUS_CONVERT_BUF_LEN] __attribute__((aligned(4)));

    for( uint16_t i = 0; i < count; i++ ){

        // NOTE: we do an internal read before we do the conversion.
        // this is so the conversion will also check if the value is changing.
        kv_i8_internal_get( &meta, hash, index, 1, buf, sizeof(buf) );

        if( type_i8_convert( meta.type, buf, type, data, data_len ) != 0 ){

            changed = TRUE;
        }
        
        status = kv_i8_array_set( hash, index, 1, buf, type_u16_size( meta.type ) );

        index++;

        if( index >= array_len ){

            break;
        }

        data += type_u16_size( type );
    }

    if( changed ){

        return CATBUS_STATUS_CHANGED;
    }

    return status;
}

int8_t catbus_i8_set_i64(
    catbus_hash_t32 hash, 
    int64_t data )
{
    return catbus_i8_set( hash, CATBUS_TYPE_INT64, &data, sizeof(data) );
}

int8_t catbus_i8_set(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    void *data,
    uint16_t data_len )
{

    return catbus_i8_array_set( hash, type, 0, 1, data, data_len );
}

int8_t catbus_i8_array_set(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t data_len )
{
    return _catbus_i8_internal_set( hash, type, index, count, data, data_len );
}

int8_t catbus_i8_get(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    void *data )
{

    return catbus_i8_array_get( hash, type, 0, 1, data );
}

int8_t catbus_i8_get_i64(
    catbus_hash_t32 hash, 
    int64_t *data)
{

    return catbus_i8_get( hash, CATBUS_TYPE_INT64, data );
}

int8_t catbus_i8_array_get(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    uint16_t index,
    uint16_t count,
    void *data )
{
    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta );

    if( status < 0 ){

        return status;
    }

    uint16_t array_len = meta.array_len + 1;

    // wrap index
    if( index > array_len ){

        index %= array_len;
    }

    uint16_t type_size = type_u16_size( type );

    // check if conversion is necessary
    if( type != meta.type ){
            
        uint8_t buf[CATBUS_CONVERT_BUF_LEN];

        for( uint16_t i = 0; i < count; i++ ){
       
            status = kv_i8_internal_get( &meta, hash, index, 1, buf, sizeof(buf) );         
            type_i8_convert( type, data, meta.type, buf, 0 );
            
            index++;

            if( index >= array_len ){

                break;
            }

            data += type_size;   
        }
    }
    // conversion not necessary, so we don't need our own loop
    else{

        status = kv_i8_internal_get( &meta, hash, index, count, data, type_size );
    }

    return 0;
}

#ifdef ENABLE_NETWORK
typedef struct{
    file_t file;
    uint32_t session_id;
    uint8_t flags;
    uint8_t timeout;
    bool close;
} file_transfer_thread_state_t;
#define FILE_SESSION_TIMEOUT            40


PT_THREAD( catbus_file_session_thread( pt_t *pt, file_transfer_thread_state_t *state ) )
{
PT_BEGIN( pt );
   
    while( state->close == FALSE ){

        TMR_WAIT( pt, 100 );
        state->timeout--;       

        if( state->timeout == 0 ){

            break;
        } 
    }

    fs_f_close( state->file );

    // clear session
    file_sessions[state->session_id] = 0;

PT_END( pt );
}


static thread_t _catbus_t_create_file_transfer_session(
    file_t file,
    uint32_t session_id,
    uint8_t flags){

    file_transfer_thread_state_t state;
    state.file          = file;
    state.session_id    = session_id;
    state.flags         = flags;
    state.timeout       = FILE_SESSION_TIMEOUT;
    state.close         = FALSE;

    thread_t t = thread_t_create( 
                    THREAD_CAST(catbus_file_session_thread),
                    PSTR("catbus_file_session"),
                    (uint8_t *)&state,
                    sizeof(state) );
    
    return t;
}


typedef struct{
    sock_addr_t raddr;
    file_t file;
    uint32_t transaction_id;
    uint32_t hash;
} file_check_thread_state_t;


PT_THREAD( catbus_file_check_thread( pt_t *pt, file_check_thread_state_t *state ) )
{
PT_BEGIN( pt );
    
    state->hash = hash_u32_start();

    while(1){

        uint8_t buf[256];

        int16_t read_len = fs_i16_read( state->file, buf, sizeof(buf) );

        // check for end of file
        if( read_len <= 0 ){

            break;
        }

        state->hash = hash_u32_partial( state->hash, buf, read_len );

        THREAD_YIELD( pt );
    }

    // send response
    catbus_msg_file_check_response_t msg;
    _catbus_v_msg_init( &msg.header, CATBUS_MSG_TYPE_FILE_CHECK_RESPONSE, state->transaction_id );
    
    msg.flags      = 0;
    msg.hash       = state->hash;
    msg.file_len   = fs_i32_get_size( state->file );

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &state->raddr );

    fs_f_close( state->file );

PT_END( pt );
}

static thread_t _catbus_t_create_file_check_session(
    sock_addr_t *raddr,
    file_t file,
    uint32_t transaction_id){

    file_check_thread_state_t state;
    state.raddr             = *raddr;
    state.file              = file;
    state.transaction_id    = transaction_id;

    thread_t t = thread_t_create( 
                    THREAD_CAST(catbus_file_check_thread),
                    PSTR("catbus_file_check"),
                    (uint8_t *)&state,
                    sizeof(state) );
    
    return t;
}

PT_THREAD( catbus_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint32_t last_lookup_check;
    last_lookup_check = tmr_u32_get_system_time_ms();

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, CATBUS_MAIN_PORT );

    sock_v_set_timeout( sock, 1 );

    // wait for device id
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &origin_id );

    if( origin_id == 0 ){

        // delay, but don't wait forever.
        TMR_WAIT( pt, 2000 );        

        // try again, but if we don't get an origin ID, just keep going.
        // if the wifi doesn't come up and we don't have an origin, we still want the USB to work.
        cfg_i8_get( CFG_PARAM_DEVICE_ID, &origin_id );

        if( origin_id == 0 ){

            origin_id = 1;
        }
    }

    // set up tag hashes
    // _catbus_v_setup_tag_hashes();

    while(1){

        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && ( list_u8_count( &name_lookup_list ) == 0 ) );

        uint16_t error = CATBUS_STATUS_OK;

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            // only do the resolve if the server is not processing a message

            if( sys_u8_get_mode() == SYS_MODE_SAFE ){

                goto end; // don't do any of this in safe mode
            }

            if( list_u8_count( &name_lookup_list ) == 0 ){

                // no lookups to process, we got here via socket timeout
                goto end; // nothing to do
            }

            if( !wifi_b_connected() ){

                // wifi not connected, we cannot do a lookup
                goto end;
            }

            // check if it is time for a hash lookup
            if( tmr_u32_elapsed_time_ms( last_lookup_check ) > CATBUS_HASH_LOOKUP_INTERVAL ){

                last_lookup_check = tmr_u32_get_system_time_ms();

                list_node_t ln = name_lookup_list.head;

                while( ln >= 0 ){

                    catbus_hash_lookup_t *lookup = list_vp_get_data( ln );
                    list_node_t next_ln = list_ln_next( ln );


                    lookup->tries--;

                    if( lookup->tries == 0 ){

                        // timeout, delete
                        list_v_remove( &name_lookup_list, ln );
                        list_v_release_node( ln );                                  

                        goto next_lookup;
                    }

                    // we are only doing a single lookup at a time
                    // this mechanism does not need to be efficient,
                    // since the lookup is recorded by the requesting 
                    // device, it should never need to ask again.

                    mem_handle_t h = mem2_h_alloc( sizeof(catbus_msg_lookup_hash_t) );

                    if( h < 0 ){

                        break; // bummer!  
                    }

                    catbus_msg_lookup_hash_t *msg = mem2_vp_get_ptr_fast( h );

                    _catbus_v_msg_init( &msg->header, CATBUS_MSG_TYPE_LOOKUP_HASH, 0 );
                    msg->count = 1;
                    msg->first_hash = lookup->hash;

                    sock_addr_t raddr = {
                        lookup->host_ip,
                        CATBUS_MAIN_PORT
                    };

                    if( sock_i16_sendto_m( sock, h, &raddr ) < 0 ){

                        // if transmission fails, bail out of the loop

                        break;
                    }

                    log_v_debug_P( PSTR("Looking up hash: 0x%08x at %d.%d.%d.%d"), 
                        lookup->hash,
                        lookup->host_ip.ip3,
                        lookup->host_ip.ip2,
                        lookup->host_ip.ip1,
                        lookup->host_ip.ip0
                    );

                next_lookup:
                    ln = next_ln;
                }            
            }
            else{

                // it is NOT time for a lookup,
                // do a short delay so we don't swamp the CPU poking the list
                TMR_WAIT( pt, 5 );
            }

            goto end;
        }

        catbus_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->meow != CATBUS_MEOW ){

            goto end;
        }

        if( header->version != CATBUS_VERSION ){

            goto end;
        }

        // filter our own messages
        if( header->origin_id == origin_id ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        // uint32_t start = tmr_u32_get_system_time_us();

        // log_v_debug_P( PSTR("%d"), header->msg_type );

        // DISCOVERY MESSAGES
        if( header->msg_type == CATBUS_MSG_TYPE_ANNOUNCE ){

            // no op
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_DISCOVER ){

            catbus_msg_discover_t *msg = (catbus_msg_discover_t *)header;

            // log_v_debug_P( PSTR("flags %d xid %lu"), msg->flags, header->transaction_id );

            // check query
            if( ( ( msg->flags & CATBUS_DISC_FLAG_QUERY_ALL ) == 0 ) &&
                ( !catbus_b_query_self( &msg->query ) ) ){

                goto end;
            }

            _catbus_v_send_announce( &raddr, msg->header.transaction_id );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_SHUTDOWN ){

            // catbus_msg_shutdown_t *msg = (catbus_msg_shutdown_t *)header;
        }

        // DATABASE ACCESS MESSAGES
        else if( header->msg_type == CATBUS_MSG_TYPE_LOOKUP_HASH ){

            catbus_msg_lookup_hash_t *msg = (catbus_msg_lookup_hash_t *)header;

            if( msg->count == 0 ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }
            else if( msg->count > CATBUS_MAX_HASH_LOOKUPS ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }

            // allocate reply message
            mem_handle_t h = mem2_h_alloc( sizeof(catbus_msg_resolved_hash_t) + ( ( msg->count - 1 ) * CATBUS_STRING_LEN ) );

            if( h < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                goto end;   
            }

            catbus_msg_resolved_hash_t *reply = mem2_vp_get_ptr( h );

            _catbus_v_msg_init( &reply->header, CATBUS_MSG_TYPE_RESOLVED_HASH, header->transaction_id );

            catbus_hash_t32 *hash_ptr = &msg->first_hash;
            catbus_string_t *str = &reply->first_string;
            reply->count = msg->count;

            while( msg->count > 0 ){

                msg->count--;

                memset( str->str, 0, CATBUS_STRING_LEN );

                uint32_t hash = LOAD32(hash_ptr);

                if( kv_i8_get_name( hash, str->str ) != KV_ERR_STATUS_OK ){
                
                    // try query tags
                    for( uint8_t i = 0; i < cnt_of_array(meta_tag_hashes); i++ ){

                        if( meta_tag_hashes[i] == hash ){

                            uint32_t meta_kv = 0;

                            if( i == 0 ){

                                meta_kv = CFG_PARAM_META_TAG_0;
                            }
                            else if( i == 1 ){

                                meta_kv = CFG_PARAM_META_TAG_1;
                            }
                            else if( i == 2 ){

                                meta_kv = CFG_PARAM_META_TAG_2;
                            }
                            else if( i == 3 ){

                                meta_kv = CFG_PARAM_META_TAG_3;
                            }
                            else if( i == 4 ){

                                meta_kv = CFG_PARAM_META_TAG_4;
                            }
                            else if( i == 5 ){

                                meta_kv = CFG_PARAM_META_TAG_5;
                            }
                            else if( i == 6 ){

                                meta_kv = CFG_PARAM_META_TAG_6;
                            }
                            else if( i == 7 ){

                                meta_kv = CFG_PARAM_META_TAG_7;
                            }

                            // retrieve string from config DB
                            cfg_i8_get( meta_kv, str->str );

                            break;
                        }
                    }
                }

                hash_ptr++;
                str++;
            }

            sock_i16_sendto_m( sock, h, 0 );
        }
        // this is a client response message, so there is no reply (this is the reply)
        else if( header->msg_type == CATBUS_MSG_TYPE_RESOLVED_HASH ){

            catbus_msg_resolved_hash_t *msg = (catbus_msg_resolved_hash_t *)header;

            if( msg->count == 0 ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }
            else if( msg->count > CATBUS_MAX_HASH_LOOKUPS ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }

            catbus_string_t *str = &msg->first_string;

            while( msg->count > 0 ){

                msg->count--;

                log_v_debug_P( PSTR("Resolved hash: %s from %d.%d.%d.%d"), 
                        str->str,
                        raddr.ipaddr.ip3,
                        raddr.ipaddr.ip2,
                        raddr.ipaddr.ip1,
                        raddr.ipaddr.ip0
                    );

                kvdb_v_set_name( str->str );

                // check for a lookup entry and delete it if needed
                catbus_hash_t32 hash = hash_u32_string( str->str );
                list_node_t ln = name_lookup_list.head;

                while( ln >= 0 ){

                    catbus_hash_lookup_t *lookup = list_vp_get_data( ln );
                    list_node_t next_ln = list_ln_next( ln );

                    if( lookup->hash == hash ){

                        // timeout, delete
                        list_v_remove( &name_lookup_list, ln );
                        list_v_release_node( ln );                                  

                        break;
                    }

                    ln = next_ln;
                }

                str++;
            }        
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_GET_KEY_META ){

            catbus_msg_get_key_meta_t *msg = (catbus_msg_get_key_meta_t *)header;

            uint16_t key_count = kv_u16_count();
            uint16_t index = msg->page * CATBUS_MAX_KEY_META;
            uint8_t page_count = ( key_count / CATBUS_MAX_KEY_META ) + 1;
            int16_t item_count = key_count - index;

            if( item_count > CATBUS_MAX_KEY_META ){

                item_count = CATBUS_MAX_KEY_META;
            }
            else if( item_count < 0 ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }

            uint16_t reply_len = sizeof(catbus_msg_key_meta_t) + ( ( item_count - 1 ) * sizeof(catbus_meta_t) );

            mem_handle_t h = mem2_h_alloc( reply_len );

            if( h < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                goto end;
            }

            catbus_msg_key_meta_t *reply = mem2_vp_get_ptr( h );

            _catbus_v_msg_init( &reply->header, CATBUS_MSG_TYPE_KEY_META, header->transaction_id );

            reply->page             = msg->page;
            reply->page_count       = page_count;
            reply->item_count       = item_count;

            catbus_meta_t *item = &reply->first_meta;

            for( uint8_t i = 0; i < item_count; i++ ){

                kv_meta_t meta;
                if( kv_i8_lookup_index_with_name( index + i, &meta ) < 0 ){

                    error = CATBUS_ERROR_KEY_NOT_FOUND;
                    mem2_v_free( h );
                    goto end;
                }

                uint16_t data_len = kv_u16_get_size_meta( &meta ) + sizeof(catbus_data_t) - 1;

                // check data len - if too large, log an error and skip
                // this prevents clients from requesting an item that will fail
                if( data_len > CATBUS_MAX_DATA ){

                    log_v_critical_P( PSTR("%s is too large for packet: %d bytes. Max: %d Skipping."), meta.name, data_len, CATBUS_MAX_DATA );

                    continue;
                }

                item->hash      = hash_u32_string( meta.name );
                item->type      = meta.type;
                item->flags     = meta.flags;
                item->count     = meta.array_len;
                item->reserved  = 0;

                item++;
            }

            // send reply
            sock_i16_sendto_m( sock, h, 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_GET_KEYS ){

            catbus_msg_get_keys_t *msg = (catbus_msg_get_keys_t *)header;

            if( msg->count == 0 ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }

            uint8_t reply_count = 0;

            catbus_hash_t32 *hash = &msg->first_hash;

            // calculate total data length of the response
            uint16_t reply_len = 0;
            kv_meta_t meta;

            for( uint8_t i = 0; i < msg->count; i++ ){

                if( kv_i8_lookup_hash( LOAD32(hash), &meta ) == 0 ){

                    uint16_t data_len = kv_u16_get_size_meta( &meta ) + sizeof(catbus_data_t) - 1;

                    // check if this item is too large for a single packet:
                    if( data_len > CATBUS_MAX_DATA ){

                        log_v_critical_P( PSTR("0x%0x is too large for packet: %d bytes"), LOAD32(hash), data_len );

                        error = CATBUS_ERROR_DATA_TOO_LARGE;
                        goto end;
                    }
                    else if( ( reply_len + data_len ) > CATBUS_MAX_DATA ){

                        // this is ok - catbus can request more items that the reply message can contain.
                        // the client is expected to handle this.

                        break;
                    }
                    
                    reply_len += data_len;
                    reply_count++;
                }       
                else{
                    
                    // this error message is *extremely* annoying
                    // log_v_warn_P( PSTR("%lu not found from: %d.%d.%d.%d:%d"), LOAD32(hash), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0, raddr.port );
                }

                hash++;
            }

            if( reply_count == 0 ){

                error = CATBUS_ERROR_KEY_NOT_FOUND;
                goto end;
            }

            mem_handle_t h = mem2_h_alloc( reply_len + sizeof(catbus_msg_key_data_t) - sizeof(catbus_data_t) );

            if( h < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                goto end;
            }

            catbus_msg_key_data_t *reply = mem2_vp_get_ptr( h) ;

            _catbus_v_msg_init( &reply->header, CATBUS_MSG_TYPE_KEY_DATA, header->transaction_id );

            hash = &msg->first_hash;
            catbus_data_t *data = &reply->first_data;

            reply->count = reply_count;

            for( uint8_t i = 0; i < reply_count; i++ ){

                if( kv_i8_lookup_hash( LOAD32(hash), &meta ) == 0 ){

                    uint16_t type_len = kv_u16_get_size_meta( &meta );

                    data->meta.hash     = LOAD32(hash);
                    data->meta.type     = meta.type;
                    data->meta.count    = meta.array_len;
                    data->meta.flags    = meta.flags;
                    data->meta.reserved = 0;

                    if( kv_i8_get( LOAD32(hash), &data->data, type_len ) != KV_ERR_STATUS_OK ){

                        error = CATBUS_ERROR_KEY_NOT_FOUND;
                        mem2_v_free( h );
                        goto end;
                    }

                    uint8_t *ptr = (uint8_t *)data;
                    ptr += ( sizeof(catbus_data_t) + ( type_len - 1 ) );
                    data = (catbus_data_t *)ptr;
                }
                else{

                    // ASSERT( FALSE ); // all requested keys were already checked, so they must exist here
                    error = CATBUS_ERROR_KEY_NOT_FOUND;
                    goto end;
                }

                hash++;
            }

            // send reply
            sock_i16_sendto_m( sock, h, 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_SET_KEYS ){

            catbus_msg_set_keys_t *msg = (catbus_msg_set_keys_t *)header;

            if( msg->count == 0 ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }

            // get reply message.
            // the set keys message is actually the same as key data.
            // so, instead of building a separate reply, we will update in place and
            // then send the existing handle.
            mem_handle_t h = sock_h_get_data_handle( sock );

            catbus_data_t *data = &msg->first_data;

            for( uint8_t i = 0; i < msg->count; i++ ){
                kv_meta_t meta;

                if( kv_i8_lookup_hash( data->meta.hash, &meta ) < 0 ){

                    error = CATBUS_ERROR_KEY_NOT_FOUND;
                    goto end;
                }

                // verify type
                if( data->meta.type != meta.type ){

                    error = CATBUS_ERROR_INVALID_TYPE;
                    goto end;
                }

                // check if read only
                if( meta.flags & KV_FLAGS_READ_ONLY ){

                    error = CATBUS_ERROR_READ_ONLY;
                    goto end;
                }

                uint16_t type_size = kv_u16_get_size_meta( &meta );
                    
                // set value
                if( kv_i8_set( data->meta.hash, &data->data, type_size ) != KV_ERR_STATUS_OK ){

                    error = CATBUS_ERROR_KEY_NOT_FOUND;
                    goto end;
                }

                if( kv_v_notify_hash_set != 0 ){

                   kv_v_notify_hash_set( data->meta.hash );
                }

                // get value, so we'll return what actually got set.
                if( kv_i8_get( data->meta.hash, &data->data, type_size ) != KV_ERR_STATUS_OK ){

                    error = CATBUS_ERROR_KEY_NOT_FOUND;
                    goto end;
                }

                uint8_t *ptr = (uint8_t *)data;

                ptr += ( sizeof(catbus_data_t) + ( type_size - 1 ) );
                data = (catbus_data_t *)ptr;
            }

            // change msg type
            _catbus_v_msg_init( header, CATBUS_MSG_TYPE_KEY_DATA, header->transaction_id );

            // transmit
            sock_i16_sendto_m( sock, h, 0 );
        }
        // FILE SYSTEM MESSAGES
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_OPEN ){

            catbus_msg_file_open_t *msg = (catbus_msg_file_open_t *)header;

            // check if session is available
            uint8_t session_id = 0;
            while( session_id < cnt_of_array(file_sessions) ){
            
                if( file_sessions[session_id] <= 0 ){

                    // session is available
                    break;
                }

                session_id++;
            }

            if( session_id >= cnt_of_array(file_sessions) ){

                error = CATBUS_ERROR_FILESYSTEM_BUSY;
                goto end;
            }

            // set file open mode
            mode_t8 mode = FS_MODE_READ_ONLY;

            if( msg->flags & CATBUS_MSG_FILE_FLAG_WRITE ){

                mode = FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND;
            }

            // attempt to open file
            file_t f = fs_f_open( msg->filename, mode );

            if( f < 0 ){

                error = CATBUS_ERROR_FILE_NOT_FOUND;
                goto end;
            }

            // create session
            thread_t t = _catbus_t_create_file_transfer_session( f, session_id, msg->flags );

            if( t < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                fs_f_close( f );
                goto end;
            }

            file_sessions[session_id] = t;

            // no errors - send confirmation on NEW socket
            catbus_msg_file_confirm_t reply;
            _catbus_v_msg_init( &reply.header, CATBUS_MSG_TYPE_FILE_CONFIRM, header->transaction_id );

            reply.flags = msg->flags;
            memcpy( reply.filename, msg->filename, sizeof(reply.filename) );
            reply.status        = 0;
            reply.session_id    = session_id;
            reply.page_size     = CATBUS_FILE_PAGE_SIZE;

            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_GET ){
            
            catbus_msg_file_get_t *msg = (catbus_msg_file_get_t *)header;

            // check that session is valid
            if( ( msg->session_id >= cnt_of_array(file_sessions) ) ||
                ( file_sessions[msg->session_id] <= 0 ) ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            file_transfer_thread_state_t *session_state = thread_vp_get_data( file_sessions[msg->session_id] );

            // check session
            if( session_state->session_id != msg->session_id ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            // reset session timeout
            session_state->timeout = FILE_SESSION_TIMEOUT;

            // seek file
            fs_v_seek( session_state->file, msg->offset );

            // check remaining file size
            int32_t file_size = fs_i32_get_size( session_state->file );
            int32_t data_len = file_size - msg->offset;

            // check if we're at the end of the file
            if( msg->offset >= file_size ){

                data_len = 0;
            }

            if( data_len < 0 ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
                goto end;
            }

            if( data_len > CATBUS_FILE_PAGE_SIZE ){

                data_len = CATBUS_FILE_PAGE_SIZE;
            }

            // alloc data
            mem_handle_t h = mem2_h_alloc( data_len + sizeof(catbus_msg_file_data_t) - 1 );

            // check allocation
            if( h < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                goto end;
            }

            catbus_msg_file_data_t *data = (catbus_msg_file_data_t *)mem2_vp_get_ptr( h );

            _catbus_v_msg_init( &data->header, CATBUS_MSG_TYPE_FILE_DATA, 0 );

            int16_t read_len = fs_i16_read( session_state->file, &data->data, data_len );

            if( read_len < 0 ){
                
                read_len = 0;
            }

            data->flags         = session_state->flags;
            data->session_id    = session_state->session_id;
            data->offset        = msg->offset;
            data->len           = read_len;

            sock_i16_sendto_m( sock, h, 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_DATA ){

            catbus_msg_file_data_t *msg = (catbus_msg_file_data_t *)header;

            // check that session is valid
            if( ( msg->session_id >= cnt_of_array(file_sessions) ) ||
                ( file_sessions[msg->session_id] <= 0 ) ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            file_transfer_thread_state_t *session_state = thread_vp_get_data( file_sessions[msg->session_id] );

            // check session
            if( session_state->session_id != msg->session_id ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            // reset session timeout
            session_state->timeout = FILE_SESSION_TIMEOUT;

            catbus_msg_file_get_t get;
            _catbus_v_msg_init( &get.header, CATBUS_MSG_TYPE_FILE_GET, header->transaction_id );    

            get.session_id  = session_state->session_id;
            get.offset      = msg->offset + msg->len;
            get.flags       = session_state->flags;
                
            if( msg->offset == fs_i32_tell( session_state->file ) ){

                // send msg sooner rather than later
                sock_i16_sendto( sock, (uint8_t *)&get, sizeof(get), 0 );

                // write to file
                fs_i16_write( session_state->file, &msg->data, msg->len );
            }
            else{

                // send reply, even though we've already received this chunk
                sock_i16_sendto( sock, (uint8_t *)&get, sizeof(get), 0 );
            }
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_CLOSE ){

            catbus_msg_file_close_t *msg = (catbus_msg_file_close_t *)header;

            // check that session is valid
            if( ( msg->session_id >= cnt_of_array(file_sessions) ) ||
                ( file_sessions[msg->session_id] <= 0 ) ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            file_transfer_thread_state_t *session_state = thread_vp_get_data( file_sessions[msg->session_id] );

            // check session
            if( session_state->session_id != msg->session_id ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }   

            // close session
            fs_f_close( session_state->file );
            thread_v_kill( file_sessions[msg->session_id] );
            file_sessions[msg->session_id] = 0;

            catbus_msg_file_ack_t reply;
            _catbus_v_msg_init( &reply.header, CATBUS_MSG_TYPE_FILE_ACK, header->transaction_id );
        
            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_DELETE ){

            catbus_msg_file_delete_t *msg = (catbus_msg_file_delete_t *)header;

            // attempt to open file
            file_t f = fs_f_open( msg->filename, FS_MODE_WRITE_OVERWRITE );

            if( f < 0 ){

                error = CATBUS_ERROR_FILE_NOT_FOUND;
                goto end;
            }

            fs_v_delete( f );            
            f = fs_f_close( f );

            // check if this is log.txt
            // if so, we want to recreate this file so we can keep logging
            if( strcmp_P( msg->filename, PSTR("log.txt") ) == 0 ){

                log_v_init(); // re-init the log file
            }

            catbus_msg_file_ack_t reply;
            _catbus_v_msg_init( &reply.header, CATBUS_MSG_TYPE_FILE_ACK, header->transaction_id );
        
            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_CHECK ){

            catbus_msg_file_check_t *msg = (catbus_msg_file_check_t *)header;

            // attempt to open file
            file_t f = fs_f_open( msg->filename, FS_MODE_READ_ONLY );

            if( f < 0 ){

                error = CATBUS_ERROR_FILE_NOT_FOUND;
                goto end;
            }

            sock_addr_t raddr;
            sock_v_get_raddr( sock, &raddr );

            thread_t t = _catbus_t_create_file_check_session(
                            &raddr,
                            f,
                            header->transaction_id);

            if( t < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                f = fs_f_close( f );
                goto end;
            }
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_LIST ){

            catbus_msg_file_list_t *msg = (catbus_msg_file_list_t *)header;

            int16_t file_count = fs_u32_get_file_count();
            int16_t index = msg->index;
            int16_t item_count = CATBUS_MAX_FILE_ENTRIES;
            
            uint16_t reply_len = sizeof(catbus_msg_file_list_data_t) + ( ( item_count - 1 ) * sizeof(catbus_file_meta_t) );

            mem_handle_t h = mem2_h_alloc( reply_len );

            if( h < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                goto end;
            }

            catbus_msg_file_list_data_t *reply = mem2_vp_get_ptr( h );

            memset( reply, 0, reply_len );

            _catbus_v_msg_init( &reply->header, CATBUS_MSG_TYPE_FILE_LIST_DATA, header->transaction_id );

            reply->filename_len     = FS_MAX_FILE_NAME_LEN;
            reply->index            = msg->index;
            reply->file_count       = file_count;

            catbus_file_meta_t *item = &reply->first_meta;

            for( uint8_t i = 0; i < item_count; i++ ){

                item[i].size = -1;
            }

            while( ( item_count > 0 ) && ( index < FS_MAX_FILES ) ){
                
                item->size = fs_i32_get_size_id( index );  

                if( item->size >= 0 ){

                    if( FS_FILE_IS_VIRTUAL( index ) ){

                        item->flags = FS_INFO_FLAGS_VIRTUAL;
                    }

                    fs_i8_get_filename_id( index, item->filename, sizeof(item->filename) );

                    item++;
                    reply->item_count++;
                    item_count--;
                }

                index++;
            }

            // send next index to client, because some indexes will be empty and 
            // we skip those.
            reply->next_index = index;

            // send reply
            sock_i16_sendto_m( sock, h, 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_ERROR ){

            // catbus_msg_error_t *msg = (catbus_msg_error_t *)header;

            // sock_addr_t raddr;
            // sock_v_get_raddr( sock, &raddr );

            // log_v_debug_P( PSTR("error: %u from %d.%d.%d.%d"), 
            //     msg->error_code,
            //     raddr.ipaddr.ip3,
            //     raddr.ipaddr.ip2,
            //     raddr.ipaddr.ip1,
            //     raddr.ipaddr.ip0 );
        }
        // unknown message type
        else{

            error = CATBUS_ERROR_UNKNOWN_MSG;
        }

        // uint32_t elapsed = tmr_u32_elapsed_time_us( start );
        
        // if( elapsed > 5000 ){

        //     log_v_debug_P( PSTR("%u:%d"), elapsed, header->msg_type );        
        // }
        
end:
    
        if( error != CATBUS_STATUS_OK ){

            if( ( error != CATBUS_ERROR_FILE_NOT_FOUND ) &&
                ( ( header->msg_type < CATBUS_MSG_DATABASE_GROUP_OFFSET ) ||
                  ( header->msg_type >= CATBUS_MSG_FILE_GROUP_OFFSET ) ) ){

                // file not found is a normal condition, so lets not log it.
                // also don't log unknown messages, it creates a lot of noise when
                // this is a normal condition when adding new protocol features

                log_v_debug_P( PSTR("error: 0x%x msg: %u"), error, header->msg_type );
            }

            // don't send unknown message errors. it just causes a ton of extra traffic
            // when adding new broadcast messages that older nodes don't recognize.
            if( error != CATBUS_ERROR_UNKNOWN_MSG ){

                catbus_msg_error_t msg;
                _catbus_v_msg_init( &msg.header, CATBUS_MSG_TYPE_ERROR, header->transaction_id );

                msg.error_code = error;

                sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
            }
        }        

        THREAD_YIELD( pt );
    }

PT_END( pt );
}


PT_THREAD( catbus_announce_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

    _catbus_v_broadcast_announce();
    TMR_WAIT( pt, 100 );
    _catbus_v_broadcast_announce();
    TMR_WAIT( pt, 100 );
    _catbus_v_broadcast_announce();
    
    while(1){

        TMR_WAIT( pt, ( CATBUS_ANNOUNCE_INTERVAL * 1000 ) + ( rnd_u16_get_int() >> 6 ) ); // add up to 1023 ms randomly

        // are we shutting down?
        if( sys_b_is_shutting_down() ){

            // we can terminate the thread, since sending announcements and links
            // is counter productive if we're about to turn off.

            THREAD_EXIT ( pt );
        }

        _catbus_v_broadcast_announce();
        TMR_WAIT( pt, 100 );
        _catbus_v_broadcast_announce();
    }

PT_END( pt );
}

#endif

PT_THREAD( catbus_shutdown_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    #ifdef ENABLE_NETWORK
    
    // broadcast shutdown messages
    
    _catbus_v_send_shutdown();

    TMR_WAIT( pt, 50 );

    _catbus_v_send_shutdown();

    TMR_WAIT( pt, 50 );

    _catbus_v_send_shutdown();

    TMR_WAIT( pt, 100 );

    _catbus_v_send_shutdown();

    TMR_WAIT( pt, 100 );

    _catbus_v_send_shutdown();
        
    #endif

PT_END( pt );
}



void catbus_v_shutdown( void ){

    // broadcast shutdown to network
    thread_t_create( THREAD_CAST(catbus_shutdown_thread),
                     PSTR("catbus_shutdown"),
                     0,
                     0 );

}

#ifdef ENABLE_NETWORK
uint64_t catbus_u64_get_origin_id( void ){

    return origin_id;
}

const catbus_hash_t32* catbus_hp_get_tag_hashes( void ){

    return meta_tag_hashes;
}
#endif



static bool is_hash_lookup_in_list( catbus_hash_t32 hash ){

    list_node_t ln = name_lookup_list.head;

    while( ln >= 0 ){

        catbus_hash_lookup_t *lookup = list_vp_get_data( ln );

        if( lookup->hash == hash ){

            // reset tries, since we obviously still want this lookup
            // lookup->tries = CATBUS_HASH_LOOKUP_TRIES;

            // let it expire?
            // callers can add it back of course.

            return TRUE;
        }

        ln = list_ln_next( ln );
    }     

    return FALSE;
}


int8_t catbus_i8_get_string_for_hash( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN], ip_addr4_t *host_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        // do not enable this function in safe mode

        return -1;
    }

    memset( name, 0, CATBUS_STRING_LEN );

    if( hash == 0 ){

        // set default string so we at least return something

        if( host_ip == 0 ){

            snprintf_P( name, CATBUS_STRING_LEN, PSTR("0x%08x"), hash );
        }
        else{

            snprintf_P( name, CATBUS_STRING_LEN, PSTR("%d.%d.%d.%d"), host_ip->ip3, host_ip->ip2, host_ip->ip1, host_ip->ip0 );
        }

        return 0;
    }

    // try to look up string, from KV database
    int8_t status = kv_i8_get_name( hash, name );

    if( status != KV_ERR_STATUS_OK ){

        // check name lookup list and if host IP is given
        if( ( host_ip != 0 ) &&
            ( list_u8_count( &name_lookup_list ) < CATBUS_MAX_HASH_RESOLVER_LOOKUPS ) ){

            // search list to see if this hash is already present
            // note that since the hash is only a couple bytes and we are using
            // the linked list, there is a lot of overhead.
            // however, we limit the size of the list and it is ephemeral,
            // lookups either resolve or time out.

            if( !is_hash_lookup_in_list( hash ) ){

                // add new lookup
                catbus_hash_lookup_t lookup = {
                    hash,
                    *host_ip,
                    CATBUS_HASH_LOOKUP_TRIES                    
                };

                list_node_t ln = list_ln_create_node( &lookup, sizeof(lookup) );

                if( ln > 0 ){

                    list_v_insert_tail( &name_lookup_list, ln );
                }
            }   
        }

        // set default string so we at least return something
        if( host_ip == 0 ){

            snprintf_P( name, CATBUS_STRING_LEN, PSTR("0x%08x"), hash );
        }
        else{

            snprintf_P( name, CATBUS_STRING_LEN, PSTR("%d.%d.%d.%d"), host_ip->ip3, host_ip->ip2, host_ip->ip1, host_ip->ip0 );
        }
    }

    return status;
}

