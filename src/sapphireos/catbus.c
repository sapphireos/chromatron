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

#include "sapphire.h"
#include "config.h"
#include "hash.h"
#include "catbus.h"
#include "random.h"

#ifdef LIB_SNTP
#include "sntp.h"
#endif

// #define NO_LOGGING
#include "logging.h"

#ifdef ENABLE_CATBUS_LINK
typedef struct{
    uint8_t flags;
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
    catbus_query_t query;
} catbus_link_state_t;
#define CATBUS_LINK_FLAGS_SOURCE        0x01
#define CATBUS_LINK_FLAGS_DEST          0x04
#define CATBUS_LINK_FLAGS_DELETE        0x80

typedef struct{
    sock_addr_t raddr;
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
    int8_t ttl;
} catbus_send_data_entry_t;

typedef struct{
    sock_addr_t raddr;
    catbus_hash_t32 dest_hash;
    uint16_t sequence;
    int8_t ttl;
} catbus_receive_data_entry_t;

static bool link_enable;
static list_t links;
static list_t send_list;
static list_t receive_cache;
static uint16_t sequence;
static uint8_t send_list_locked;
#endif

static socket_t sock;
static thread_t file_session_thread = -1;

static catbus_hash_t32 meta_tag_hashes[CATBUS_QUERY_LEN];
// start of adjustable tags through the add/rm interface
// tags before this index must be set directly
#define CATBUS_META_TAGS_START 2 


PT_THREAD( catbus_server_thread( pt_t *pt, void *state ) );
PT_THREAD( catbus_announce_thread( pt_t *pt, void *state ) );


static uint32_t test_array[8];
KV_SECTION_META kv_meta_t catbus_kv[] = {
    { SAPPHIRE_TYPE_UINT32,  KV_ARRAY_LEN(8), 0, &test_array,  0, "kv_test_array" },
};


static int8_t _catbus_i8_meta_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len );

KV_SECTION_META kv_meta_t catbus_metatags_kv[] = {
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_name" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_location" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_0" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_1" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_2" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_3" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_4" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler,  "meta_tag_5" },

    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler, "meta_cmd_add" },
    { SAPPHIRE_TYPE_STRING32, 0, 0, 0, _catbus_i8_meta_handler, "meta_cmd_rm" },
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

#ifdef ENABLE_CATBUS_LINK
static uint16_t links_vfile_handler(
    vfile_op_t8 op,
    uint32_t pos,
    void *ptr,
    uint16_t len )
{

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            list_u16_flatten( &links, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &links );
            break;

        case FS_VFILE_OP_DELETE:
            break;

        default:
            len = 0;
            break;
    }

    return len;
}

static uint16_t sendlist_vfile_handler(
    vfile_op_t8 op,
    uint32_t pos,
    void *ptr,
    uint16_t len )
{

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            list_u16_flatten( &send_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &send_list );
            break;

        case FS_VFILE_OP_DELETE:
            break;

        default:
            len = 0;
            break;
    }

    return len;
}

static uint16_t receive_cache_vfile_handler(
    vfile_op_t8 op,
    uint32_t pos,
    void *ptr,
    uint16_t len )
{

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            list_u16_flatten( &receive_cache, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &receive_cache );
            break;

        case FS_VFILE_OP_DELETE:
            break;

        default:
            len = 0;
            break;
    }

    return len;
}
#endif

static void _catbus_v_setup_tag_hashes( void ){
    
    for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

        char s[CATBUS_STRING_LEN];
        memset( s, 0, sizeof(s) );

        // read tag name hash from flash lookup
        catbus_hash_t32 hash;
        memcpy_P( &hash, &meta_tag_names[i], sizeof(hash) );

        if( cfg_i8_get( hash, s ) == 0 ){

            meta_tag_hashes[i] = hash_u32_string( s );
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
            catbus_hash_t32 hash;
            memcpy_P( &hash, &meta_tag_names[i], sizeof(hash) );

            cfg_v_set( hash, tag );

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
            catbus_hash_t32 hash;
            memcpy_P( &hash, &meta_tag_names[i], sizeof(hash) );

            cfg_v_erase( hash );
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

    #ifdef ENABLE_CATBUS_LINK
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        link_enable = FALSE;
    }
    else{

        link_enable = TRUE;

        list_v_init( &links );
        list_v_init( &send_list );
        list_v_init( &receive_cache );

        fs_f_create_virtual( PSTR("kvlinks"), links_vfile_handler );
        fs_f_create_virtual( PSTR("kvrxcache"), receive_cache_vfile_handler );
        fs_f_create_virtual( PSTR("kvsend"), sendlist_vfile_handler );
    }
    #endif

    thread_t_create( catbus_server_thread,
                     PSTR("catbus_server"),
                     0,
                     0 );

    thread_t_create( catbus_announce_thread,
                     PSTR("catbus_announce"),
                     0,
                     0 );
}

void catbus_v_set_options( uint32_t options ){

    #ifdef ENABLE_CATBUS_LINK
    if( options & CATBUS_OPTION_LINK_DISABLE ){
        
        link_enable = FALSE;
    }
    else{

        link_enable = TRUE;
    }
    #endif
}

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

    cfg_i8_get( CFG_PARAM_DEVICE_ID, &header->origin_id );

}

static void _catbus_v_get_query( catbus_query_t *query ){

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

static bool _catbus_b_query_self( catbus_query_t *query ){

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

#ifdef ENABLE_CATBUS_LINK
static bool _catbus_b_hash_in_query( catbus_hash_t32 hash, catbus_query_t *query ){

    for( uint8_t i = 0; i < cnt_of_array(query->tags); i++ ){

        if( hash == query->tags[i] ){

            return TRUE;
        }
    }

    return FALSE;
}

static bool _catbus_b_compare_queries( catbus_query_t *query1, catbus_query_t *query2 ){

    for( uint8_t i = 0; i < cnt_of_array(query1->tags); i++ ){
        
        if( !_catbus_b_hash_in_query( query1->tags[i], query2 ) ){

            return FALSE;
        }
    }

    for( uint8_t i = 0; i < cnt_of_array(query2->tags); i++ ){
        
        if( !_catbus_b_hash_in_query( query2->tags[i], query1 ) ){

            return FALSE;
        }
    }

    return TRUE;
}
#endif

static void _catbus_v_send_announce( sock_addr_t *raddr, uint32_t discovery_id ){

    mem_handle_t h = mem2_h_alloc( sizeof(catbus_msg_announce_t) );

    if( h < 0 ){

        return;
    }

    catbus_msg_announce_t *msg = mem2_vp_get_ptr( h );
    _catbus_v_msg_init( &msg->header, CATBUS_MSG_TYPE_ANNOUNCE, discovery_id );

    msg->flags = 0;
    msg->data_port = sock_u16_get_lport( sock );

    _catbus_v_get_query( &msg->query );

    sock_i16_sendto_m( sock, h, raddr );
}

#ifdef ENABLE_CATBUS_LINK
static void _catbus_v_add_to_send_list( catbus_hash_t32 source_hash, catbus_hash_t32 dest_hash, sock_addr_t *raddr ){

    // check if entry already exists
    list_node_t ln = send_list.head;

    while( ln >= 0 ){

        catbus_send_data_entry_t *entry = (catbus_send_data_entry_t *)list_vp_get_data( ln );

        if( ( entry->source_hash == source_hash ) && 
            ( entry->dest_hash == dest_hash ) && 
            ( memcmp( raddr, &entry->raddr, sizeof(sock_addr_t) ) == 0 ) ){

            // reset TTL
            entry->ttl = 32;

            return;
        }

        ln = list_ln_next( ln );
    }

    // create new entry
    catbus_send_data_entry_t entry;
    entry.source_hash   = source_hash;
    entry.dest_hash     = dest_hash;
    entry.raddr         = *raddr;
    entry.ttl           = 32;

    ln = list_ln_create_node2( &entry, sizeof(entry), MEM_TYPE_CATBUS_SEND );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &send_list, ln );
}

static bool _catbus_b_compare_links( catbus_link_state_t *state, catbus_link_t link ){

    catbus_link_state_t *state2 = list_vp_get_data( link );

    if( state->flags != state2->flags ){

        return FALSE;
    }

    if( state->source_hash != state2->source_hash ){

        return FALSE;
    }

    if( state->dest_hash != state2->dest_hash ){

        return FALSE;
    }

    if( !_catbus_b_compare_queries( &state->query, &state2->query ) ){

        return FALSE;
    }


    return TRUE;
}

static catbus_link_t _catbus_l_create_link( 
    bool source, 
    catbus_hash_t32 source_hash, 
    catbus_hash_t32 dest_hash, 
    catbus_query_t *query ){

    catbus_link_state_t state;

    state.flags = 0;

    if( source ){

        state.flags |= CATBUS_LINK_FLAGS_SOURCE;
    }

    state.source_hash       = source_hash;
    state.dest_hash         = dest_hash;
    state.query             = *query;
    
    // check if we already have this link
    catbus_link_t ln = links.head;

    while( ln > 0 ){

        if( _catbus_b_compare_links( &state, ln ) ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    ln = list_ln_create_node2( &state, sizeof(state), MEM_TYPE_CATBUS_LINK );

    if( ln < 0 ){

        return -1;
    }

    list_v_insert_tail( &links, ln );

    return ln;
}


static void _catbus_v_send_link( catbus_link_t link ){

    mem_handle_t h = mem2_h_alloc( sizeof(catbus_msg_link_t) );

    if( h < 0 ){

        return;
    }

    catbus_link_state_t *state = list_vp_get_data( link );

    catbus_msg_link_t *msg = mem2_vp_get_ptr( h );

    _catbus_v_msg_init( &msg->header, CATBUS_MSG_TYPE_LINK, 0 );

    msg->source_hash    = state->source_hash;
    msg->dest_hash      = state->dest_hash;
    msg->flags          = state->flags;
    msg->query          = state->query;

    sock_addr_t raddr;
    raddr.ipaddr    = ip_a_addr(255,255,255,255);
    raddr.port      = CATBUS_DISCOVERY_PORT;

    sock_i16_sendto_m( sock, h, &raddr );
}
#endif


int8_t catbus_i8_set(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    void *data )
{

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        return status;
    }

    uint8_t buf[CATBUS_STRING_LEN];
    type_i8_convert( meta.type, buf, type, data );

    // log_v_debug_P( PSTR("set: %d -> %d"), type, meta.type);

    status = kv_i8_set( hash, buf, type_u16_size( meta.type ) );

    if( status == 0 ){

        catbus_i8_publish( hash );
    }

    return status;
}

int8_t catbus_i8_get(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    void *data ){

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        return status;
    }

    uint8_t buf[CATBUS_STRING_LEN];

    status = kv_i8_get( hash, buf, sizeof(buf) );

    if( status < 0 ){

        return status;
    }

    type_i8_convert( type, data, meta.type, buf );

    return 0;
}

#ifdef ENABLE_CATBUS_LINK
catbus_link_t catbus_l_send( 
    catbus_hash_t32 source_hash, 
    catbus_hash_t32 dest_hash, 
    catbus_query_t *dest_query ){

    if( !link_enable ){

        return -1;
    }

    return _catbus_l_create_link( TRUE, source_hash, dest_hash, dest_query );
}

catbus_link_t catbus_l_recv( 
    catbus_hash_t32 dest_hash, 
    catbus_hash_t32 source_hash, 
    catbus_query_t *source_query ){

    if( !link_enable ){

        return -1;
    }

    return _catbus_l_create_link( FALSE, source_hash, dest_hash, source_query );
}

// destroy all links created on this node
void catbus_v_purge_links( void ){

    if( !link_enable ){

        return;
    }

    list_node_t link = links.head;

    while( link > 0 ){

        catbus_link_state_t *state = list_vp_get_data( link );

        state->flags |= CATBUS_LINK_FLAGS_DELETE;

        link = list_ln_next( link );
    }
}


typedef struct{
    catbus_msg_link_data_t msg;
    list_node_t ln;
} publish_thread_state_t;

PT_THREAD( publish_thread( pt_t *pt, publish_thread_state_t *state ) )
{
PT_BEGIN( pt );

    send_list_locked++;
        
    state->ln = send_list.head;

    while( state->ln > 0 ){

        catbus_send_data_entry_t *send_state = (catbus_send_data_entry_t *)list_vp_get_data( state->ln );

        if( send_state->source_hash != state->msg.source_hash ){

            goto end;
        }

        state->msg.dest_hash = send_state->dest_hash;

        sock_i16_sendto( sock, (uint8_t *)&state->msg, sizeof(state->msg), &send_state->raddr );

        TMR_WAIT( pt, 2 );

end:
        state->ln = list_ln_next( state->ln );
    }

    send_list_locked--;
    
PT_END( pt );
}
#endif

int8_t catbus_i8_publish( catbus_hash_t32 hash ){

    #ifdef ENABLE_CATBUS_LINK
    if( !link_enable ){

        return 0;
    }

    // check if safe mode
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){    

        return 0;
    }

    if( kv_v_notify_hash_set != 0 ){ 
    
        kv_v_notify_hash_set( hash );
    }

    publish_thread_state_t state;

    _catbus_v_msg_init( &state.msg.header, CATBUS_MSG_TYPE_LINK_DATA, 0 );
    state.msg.flags = 0;

    #ifdef LIB_SNTP
    state.msg.ntp_timestamp = sntp_t_now();

    if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){
        
        state.msg.flags |= CATBUS_MSG_DATA_FLAG_TIME_SYNC;
    }
    #endif

    _catbus_v_get_query( &state.msg.source_query );

    state.msg.source_hash = hash;
    
    if( catbus_i8_get( hash, CATBUS_TYPE_INT32, &state.msg.data ) < 0 ){

        return -1;
    }

    sequence++;
    state.msg.sequence = sequence;

    // we're not checking for success, since there is nothing we
    // can do about it.
    thread_t_create( THREAD_CAST(publish_thread),
                     PSTR("catbus_publish"),
                     &state,
                     sizeof(state) );
    
    #endif

    return 0;   
}


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

    file_session_thread = -1;

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

    uint16_t data_port = CATBUS_DISCOVERY_PORT;
    // cfg_i8_get( CFG_PARAM_CATBUS_DATA_PORT, &data_port );

    // if( data_port == 0 ){

        // data_port = CATBUS_DISCOVERY_PORT;

        cfg_v_set( CFG_PARAM_CATBUS_DATA_PORT, &data_port );
    // }


    // create socket
    sock = sock_s_create( SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, data_port );

    sock_v_set_timeout( sock, 1 );

    // set up tag hashes
    _catbus_v_setup_tag_hashes();

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        catbus_header_t *header = sock_vp_get_data( sock );
        uint16_t error = CATBUS_ERROR_OK;

        // verify message
        if( header->meow != CATBUS_MEOW ){

            goto end;
        }

        if( header->version != CATBUS_VERSION ){

            goto end;
        }

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
                ( !_catbus_b_query_self( &msg->query ) ) ){

                goto end;
            }

            // log_v_debug_P( PSTR("disc") );

            sock_addr_t raddr;
            sock_v_get_raddr( sock, &raddr );

            _catbus_v_send_announce( &raddr, msg->header.transaction_id );
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

            catbus_hash_t32 *hash = &msg->first_hash;
            catbus_string_t *str = &reply->first_string;
            reply->count = msg->count;

            while( msg->count > 0 ){

                msg->count--;

                if( kv_i8_get_name( *hash, str->str ) != KV_ERR_STATUS_OK ){

                    // try query tags
                    for( uint8_t i = 0; i < cnt_of_array(meta_tag_hashes); i++ ){

                        if( meta_tag_hashes[i] == *hash ){

                            // retrieve string from config DB
                            cfg_i8_get( CFG_PARAM_META_TAG_0 + i, str->str );

                            break;
                        }
                    }
                }

                hash++;
                str++;
            }

            sock_i16_sendto_m( sock, h, 0 );
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
                if( kv_i8_lookup_index( index + i, &meta, KV_META_FLAGS_GET_NAME ) < 0 ){

                    error = CATBUS_ERROR_KEY_NOT_FOUND;
                    mem2_v_free( h );
                    goto end;
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

                if( kv_i8_lookup_hash( *hash, &meta, 0 ) == 0 ){

                    uint16_t data_len = kv_u16_get_size_meta( &meta ) + sizeof(catbus_data_t) - 1;
                    
                    if( ( reply_len + data_len ) > CATBUS_MAX_DATA ){

                        break;
                    }

                    reply_len += data_len;
                    reply_count++;
                }       
                else{

                    log_v_debug_P( PSTR("%lu not found"), *hash );
                }         

                hash++;
            }

            if( reply_len > CATBUS_MAX_DATA ){

                error = CATBUS_ERROR_PROTOCOL_ERROR;
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

                if( kv_i8_lookup_hash( *hash, &meta, 0 ) == 0 ){

                    uint16_t type_len = kv_u16_get_size_meta( &meta );

                    data->meta.hash     = *hash;
                    data->meta.type     = meta.type;
                    data->meta.count    = meta.array_len;
                    data->meta.flags    = meta.flags;
                    data->meta.reserved = 0;

                    if( kv_i8_get( *hash, &data->data, type_len ) != KV_ERR_STATUS_OK ){

                        error = CATBUS_ERROR_KEY_NOT_FOUND;
                        mem2_v_free( h );
                        goto end;
                    }

                    uint8_t *ptr = (uint8_t *)data;
                    ptr += ( sizeof(catbus_data_t) + ( type_len - 1 ) );
                    data = (catbus_data_t *)ptr;
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

                if( kv_i8_lookup_hash( data->meta.hash, &meta, 0 ) < 0 ){

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

        #ifdef ENABLE_CATBUS_LINK
        // LINK SYSTEM MESSAGES
        else if( header->msg_type == CATBUS_MSG_TYPE_LINK ){

            if( !link_enable ){

                goto end;
            }

            catbus_msg_link_t *msg = (catbus_msg_link_t *)header;

            if( ( msg->flags & CATBUS_LINK_FLAGS_DEST ) == 0 ){

                // check if we match query
                if( !_catbus_b_query_self( &msg->query ) ){

                    goto end;
                }
            }

            // check link type

            // source link 
            if( msg->flags & CATBUS_LINK_FLAGS_SOURCE ){

                // check if we have the destination key
                if( kv_i16_search_hash( msg->dest_hash ) < 0 ){

                    goto end;
                }

                // change link flags and echo message back to sender
                msg->flags = CATBUS_LINK_FLAGS_DEST;

                // update header
                _catbus_v_msg_init( header, CATBUS_MSG_TYPE_LINK, header->transaction_id );

                // send reply message
                sock_i16_sendto( sock, (uint8_t *)msg, sizeof(catbus_msg_link_t), 0 );   
            }
            // receiver link
            else{

                // check if we have the source key:
                if( kv_i16_search_hash( msg->source_hash ) < 0 ){

                    goto end;
                }

                sock_addr_t raddr;
                sock_v_get_raddr( sock, &raddr );

                _catbus_v_add_to_send_list( msg->source_hash, msg->dest_hash, &raddr );
            }
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_LINK_DATA ){

            if( !link_enable ){

                goto end;
            }

            catbus_msg_link_data_t *msg = (catbus_msg_link_data_t *)header;

            sock_addr_t raddr;
            sock_v_get_raddr( sock, &raddr );

            int32_t cached_sequence = -1;

            // look for cache entry
            list_node_t ln = receive_cache.head;

            while( ln > 0 ){

                catbus_receive_data_entry_t *entry = (catbus_receive_data_entry_t *)list_vp_get_data( ln );

                if( ip_b_addr_compare( entry->raddr.ipaddr, raddr.ipaddr ) &&
                    ( entry->raddr.port == raddr.port ) &&
                    ( entry->dest_hash == msg->dest_hash ) ){

                    cached_sequence = entry->sequence;

                    // update recorded sequence
                    entry->sequence = msg->sequence;

                    // reset ttl
                    entry->ttl = 32;

                    break;
                }

                ln = list_ln_next( ln );
            }

            // no entry exists
            if( ln < 0 ){

                // create entry
                catbus_receive_data_entry_t entry;
                entry.raddr         = raddr;
                entry.dest_hash     = msg->dest_hash;
                entry.sequence      = msg->sequence;
                entry.ttl           = 32;

                ln = list_ln_create_node2( &entry, sizeof(entry), MEM_TYPE_CATBUS_RX_CACHE );     
                
                if( ln > 0 ){           

                    list_v_insert_tail( &receive_cache, ln );
                }
            }

            if( msg->sequence != cached_sequence ){

                catbus_i8_set( msg->dest_hash, CATBUS_TYPE_INT32, (void *)&msg->data.data );

                if( kv_v_notify_hash_set != 0 ){

                    kv_v_notify_hash_set( msg->dest_hash );                    
                }
            }
        }
        #endif

        // FILE SYSTEM MESSAGES
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_OPEN ){

            catbus_msg_file_open_t *msg = (catbus_msg_file_open_t *)header;

            // check if session is available
            if( file_session_thread > 0 ){

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

            uint32_t session_id = ( (uint32_t)rnd_u16_get_int() << 16 ) | rnd_u16_get_int();

            // create session
            thread_t t = _catbus_t_create_file_transfer_session( f, session_id, msg->flags );

            if( t < 0 ){

                error = CATBUS_ERROR_ALLOC_FAIL;
                fs_f_close( f );
                goto end;
            }

            file_session_thread = t;

            // no errors - send confirmation on NEW socket
            catbus_msg_file_confirm_t reply;
            _catbus_v_msg_init( &reply.header, CATBUS_MSG_TYPE_FILE_CONFIRM, header->transaction_id );

            reply.flags = msg->flags;
            memcpy( reply.filename, msg->filename, sizeof(reply.filename) );
            reply.status        = 0;
            reply.session_id    = session_id;
            reply.page_size     = CATBUS_MAX_DATA;

            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_GET ){
            
            catbus_msg_file_get_t *msg = (catbus_msg_file_get_t *)header;

            // get file session
            if( file_session_thread < 0 ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            file_transfer_thread_state_t *session_state = thread_vp_get_data( file_session_thread );

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

            if( data_len > CATBUS_MAX_DATA ){

                data_len = CATBUS_MAX_DATA;
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

            // get file session
            if( file_session_thread < 0 ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            file_transfer_thread_state_t *session_state = thread_vp_get_data( file_session_thread );

            // check session
            if( session_state->session_id != msg->session_id ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            // reset session timeout
            session_state->timeout = FILE_SESSION_TIMEOUT;

            if( msg->offset == fs_i32_tell( session_state->file ) ){

                catbus_msg_file_get_t get;
                _catbus_v_msg_init( &get.header, CATBUS_MSG_TYPE_FILE_GET, header->transaction_id );
            
                // send msg sooner rather than later
                get.session_id  = session_state->session_id;
                get.offset      = msg->offset + msg->len;
                get.flags       = session_state->flags;
                
                sock_i16_sendto( sock, (uint8_t *)&get, sizeof(get), 0 );

                // write to file
                fs_i16_write( session_state->file, &msg->data, msg->len );
            }
        }
        else if( header->msg_type == CATBUS_MSG_TYPE_FILE_CLOSE ){

            catbus_msg_file_close_t *msg = (catbus_msg_file_close_t *)header;

            // get file session
            if( file_session_thread < 0 ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }

            file_transfer_thread_state_t *session_state = thread_vp_get_data( file_session_thread );

            // check session
            if( session_state->session_id != msg->session_id ){

                log_v_debug_P( PSTR("invalid file session") );
                error = CATBUS_ERROR_INVALID_FILE_SESSION;
                goto end;
            }   

            // close session
            fs_f_close( session_state->file );
            thread_v_kill( file_session_thread );
            file_session_thread = -1;

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
        // unknown message type
        else{

            error = CATBUS_ERROR_UNKNOWN_MSG;
        }

end:
    
        if( error != CATBUS_ERROR_OK ){

            if( error != CATBUS_ERROR_FILE_NOT_FOUND ){
                // file not found is a normal condition, so lets not log it.

                log_v_debug_P( PSTR("error: %u msg: %u"), error, header->msg_type );
            }

            catbus_msg_error_t msg;
            _catbus_v_msg_init( &msg.header, CATBUS_MSG_TYPE_ERROR, header->transaction_id );

            msg.error_code = error;

            sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
        }        

        THREAD_YIELD( pt );
    }

PT_END( pt );
}


PT_THREAD( catbus_announce_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    #ifdef ENABLE_CATBUS_LINK
    static list_node_t ln;
    static list_node_t next_ln;
    #endif

    while(1){

        TMR_WAIT( pt, 4000 + ( rnd_u16_get_int() >> 6 ) ); // add up to 1023 ms randomly

        // check if there is received data pending
        THREAD_WAIT_WHILE( pt, sock_i16_get_bytes_read( sock) > 0 );
        // The reason to wait is to avoid a race condition that arises in
        // using one socket for send and receive between two threads.
        // The socket only stores one remote socket address.  When you send,
        // it writes the new remote address to the socket state.  If had
        // received data waiting, you will have overrwritten the remote address.
        // If you then attempt to reply, you will end up sending to the wrong
        // socket.
        // The easiest way to avoid this is just to not share sockets between
        // threads.  But if you don't want to waste the memory on that,
        // this is the other way to do it.

        sock_addr_t raddr;

        raddr.ipaddr = ip_a_addr(255,255,255,255);
        raddr.port = CATBUS_DISCOVERY_PORT;

        _catbus_v_send_announce( &raddr, 0 );


        #ifdef ENABLE_CATBUS_LINK
        TMR_WAIT( pt, 2 );

        if( !link_enable ){

            continue;
        }

        // process link system periodic tasks        

        catbus_hash_t32 hashes_published[16];
        uint8_t hashes_published_index = 0;
        memset( hashes_published, 0, sizeof(hashes_published) );
        
        ln = send_list.head;
        next_ln = -1;

        while( ln > 0 ){

            next_ln = list_ln_next( ln );

            catbus_send_data_entry_t *state = (catbus_send_data_entry_t *)list_vp_get_data( ln );
            
            // check if we've already published this one
            for( uint8_t i = 0; i < hashes_published_index; i++ ){

                if( hashes_published[i] == state->source_hash ){

                    goto next_send;
                }
            }

            if( hashes_published_index < cnt_of_array(hashes_published) ){

                hashes_published[hashes_published_index] = state->source_hash;
                hashes_published_index++;
            }

            catbus_i8_publish( state->source_hash );

            TMR_WAIT( pt, 2 );
             
next_send:
            ln = next_ln;
        }  

        // check for deleted links
        ln = links.head;
        next_ln = -1;

        while( ln > 0 ){

            next_ln = list_ln_next( ln );
            
            catbus_link_state_t *state = list_vp_get_data( ln );

            if( state->flags & CATBUS_LINK_FLAGS_DELETE ){

                list_v_remove( &links, ln );
                list_v_release_node( ln );
            }

            ln = next_ln;
        }

        // broadcast links to network    
        ln = links.head;

        while( ln > 0 ){

            // send link to network
            _catbus_v_send_link( ln );

            TMR_WAIT( pt, 5 );

            ln = list_ln_next( ln );
        }

        THREAD_WAIT_WHILE( pt, send_list_locked );

        // expire any send entries
        ln = send_list.head;
        next_ln = -1;

        while( ln > 0 ){

            next_ln = list_ln_next( ln );

            catbus_send_data_entry_t *entry = (catbus_send_data_entry_t *)list_vp_get_data( ln );

            entry->ttl -= 4;

            if( entry->ttl < 0 ){

                list_v_remove( &send_list, ln );
                list_v_release_node( ln );
            } 

            ln = next_ln;
        }  

        // expire any cache entries
        ln = receive_cache.head;
        next_ln = -1;

        while( ln > 0 ){

            next_ln = list_ln_next( ln );

            catbus_receive_data_entry_t *entry = (catbus_receive_data_entry_t *)list_vp_get_data( ln );

            entry->ttl -= 4;

            if( entry->ttl < 0 ){

                list_v_remove( &receive_cache, ln );
                list_v_release_node( ln );
            } 

            ln = next_ln;
        }  
        #endif
    }

PT_END( pt );
}
