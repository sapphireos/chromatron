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

#ifndef __CATBUS_H
#define __CATBUS_H

#include "catbus_common.h"
#include "catbus_types.h"
#include "catbus_link.h"
#include "ntp.h"
#include "list.h"
#include "udp.h"

#define CATBUS_ANNOUNCE_PORT                44631
#define CATBUS_MAIN_PORT                    44632
#define CATBUS_VERSION                      1
#define CATBUS_MEOW                         0x574f454d // 'MEOW'

#define CATBUS_ANNOUNCE_MCAST_ADDR          239,43,96,30

#define CATBUS_ANNOUNCE_INTERVAL            24
#define CATBUS_MAX_FILE_SESSIONS            8

typedef struct __attribute__((packed)){
    uint32_t meow;
    uint8_t msg_type;
    uint64_t origin_id;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    catbus_hash_t32 universe;
    uint32_t transaction_id;
} catbus_header_t;

#define CATBUS_MSG_GENERAL_GROUP_OFFSET         1
#define CATBUS_MSG_DISCOVERY_GROUP_OFFSET       10
#define CATBUS_MSG_DATABASE_GROUP_OFFSET        20
#define CATBUS_MSG_FILE_GROUP_OFFSET            60

#define CATBUS_STATUS_CHANGED                   1
#define CATBUS_STATUS_OK                        0
#define CATBUS_ERROR_UNKNOWN_MSG                0xfffe
#define CATBUS_ERROR_PROTOCOL_ERROR             0x0001
#define CATBUS_ERROR_ALLOC_FAIL                 0x0002
#define CATBUS_ERROR_KEY_NOT_FOUND              0x0003
#define CATBUS_ERROR_INVALID_TYPE               0x0004
#define CATBUS_ERROR_READ_ONLY                  0x0005
#define CATBUS_ERROR_GENERIC_ERROR              0x0006
#define CATBUS_ERROR_DATA_TOO_LARGE             0x0007

#define CATBUS_ERROR_FILE_NOT_FOUND             0x0101
#define CATBUS_ERROR_FILESYSTEM_FULL            0x0102
#define CATBUS_ERROR_FILESYSTEM_BUSY            0x0103
#define CATBUS_ERROR_INVALID_FILE_SESSION       0x0104

// #define CATBUS_MAX_DATA                     ( UDP_MAX_LEN - ( sizeof(catbus_header_t) + 1 + ( sizeof(catbus_data_t) - 1 ) ) )
#define CATBUS_MAX_DATA                         544
#define CATBUS_FILE_PAGE_SIZE                   512

// GENERAL

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint16_t error_code;
}  catbus_msg_error_t;
#define CATBUS_MSG_TYPE_ERROR                   ( 0 + CATBUS_MSG_GENERAL_GROUP_OFFSET )


// DISCOVERY
#define CATBUS_DISC_FLAG_QUERY_ALL              0x01

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    uint16_t data_port;
    catbus_query_t query;
} catbus_msg_announce_t;
#define CATBUS_MSG_TYPE_ANNOUNCE                ( 0 + CATBUS_MSG_DISCOVERY_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    catbus_query_t query;
} catbus_msg_discover_t;
#define CATBUS_MSG_TYPE_DISCOVER                ( 1 + CATBUS_MSG_DISCOVERY_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
} catbus_msg_shutdown_t;
#define CATBUS_MSG_TYPE_SHUTDOWN                ( 2 + CATBUS_MSG_DISCOVERY_GROUP_OFFSET )


// DATABASE

#define CATBUS_MAX_HASH_LOOKUPS                 16
#define CATBUS_MAX_KEY_ITEM_COUNT               32
#define CATBUS_MAX_KEY_META                     64

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t count;
    catbus_hash_t32 first_hash; // additional hashes may follow
} catbus_msg_lookup_hash_t;
#define CATBUS_MSG_TYPE_LOOKUP_HASH             ( 1 + CATBUS_MSG_DATABASE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t count;
    catbus_string_t first_string; // additional strings may follow
} catbus_msg_resolved_hash_t;
#define CATBUS_MSG_TYPE_RESOLVED_HASH           ( 2 + CATBUS_MSG_DATABASE_GROUP_OFFSET )


typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint16_t page;
} catbus_msg_get_key_meta_t;
#define CATBUS_MSG_TYPE_GET_KEY_META            ( 3 + CATBUS_MSG_DATABASE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint16_t page;
    uint16_t page_count;
    uint8_t item_count;
    catbus_meta_t first_meta; // additional meta items may follow
} catbus_msg_key_meta_t;
#define CATBUS_MSG_TYPE_KEY_META                ( 4 + CATBUS_MSG_DATABASE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t count;
    catbus_hash_t32 first_hash; // additional hashes may follow
} catbus_msg_get_keys_t;
#define CATBUS_MSG_TYPE_GET_KEYS                ( 5 + CATBUS_MSG_DATABASE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t count;
    catbus_data_t first_data; // additional data may follow
} catbus_msg_set_keys_t;
#define CATBUS_MSG_TYPE_SET_KEYS                ( 6 + CATBUS_MSG_DATABASE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t count;
    catbus_data_t first_data; // additional data may follows
} catbus_msg_key_data_t;
#define CATBUS_MSG_TYPE_KEY_DATA                ( 7 + CATBUS_MSG_DATABASE_GROUP_OFFSET )

// FILE
typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    char filename[CATBUS_STRING_LEN];
} catbus_msg_file_open_t;
#define CATBUS_MSG_TYPE_FILE_OPEN                ( 1 + CATBUS_MSG_FILE_GROUP_OFFSET )
#define CATBUS_MSG_FILE_FLAG_READ               0x01
#define CATBUS_MSG_FILE_FLAG_WRITE              0x02

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    char filename[CATBUS_STRING_LEN];
    uint8_t status;
    uint32_t session_id;
    uint16_t page_size;
} catbus_msg_file_confirm_t;
#define CATBUS_MSG_TYPE_FILE_CONFIRM             ( 2 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    uint32_t session_id;
    int32_t offset;
    uint16_t len;
    uint8_t data; // additional data may follow
} catbus_msg_file_data_t;
#define CATBUS_MSG_TYPE_FILE_DATA                ( 3 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    uint32_t session_id;
    int32_t offset;
} catbus_msg_file_get_t;
#define CATBUS_MSG_TYPE_FILE_GET                 ( 4 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    uint32_t session_id;
} catbus_msg_file_close_t;
#define CATBUS_MSG_TYPE_FILE_CLOSE               ( 5 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    char filename[CATBUS_STRING_LEN];
} catbus_msg_file_delete_t;
#define CATBUS_MSG_TYPE_FILE_DELETE              ( 6 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
} catbus_msg_file_ack_t;
#define CATBUS_MSG_TYPE_FILE_ACK                 ( 7 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    char filename[CATBUS_STRING_LEN];
} catbus_msg_file_check_t;
#define CATBUS_MSG_TYPE_FILE_CHECK               ( 8 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    catbus_hash_t32 hash;
    uint32_t file_len;
} catbus_msg_file_check_response_t;
#define CATBUS_MSG_TYPE_FILE_CHECK_RESPONSE      ( 9 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint16_t index;
} catbus_msg_file_list_t;
#define CATBUS_MSG_TYPE_FILE_LIST                ( 10 + CATBUS_MSG_FILE_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t filename_len;
    uint16_t file_count;
    uint16_t index;
    uint16_t next_index;
    uint8_t item_count;
    catbus_file_meta_t first_meta;
} catbus_msg_file_list_data_t;
#define CATBUS_MSG_TYPE_FILE_LIST_DATA           ( 11 + CATBUS_MSG_FILE_GROUP_OFFSET )
#define CATBUS_MAX_FILE_ENTRIES                  ( CATBUS_MAX_DATA / sizeof(catbus_file_meta_t) )


void catbus_v_init( void );

int8_t catbus_i8_set_i64(
    catbus_hash_t32 hash, 
    int64_t data );

int8_t catbus_i8_set(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    void *data,
    uint16_t data_len );

int8_t catbus_i8_array_set(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t data_len );


int8_t catbus_i8_get_i64(
    catbus_hash_t32 hash, 
    int64_t *data );

int8_t catbus_i8_get(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    void *data );

int8_t catbus_i8_array_get(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    uint16_t index,
    uint16_t count,
    void *data );

void catbus_v_set_options( uint32_t options );

void catbus_v_shutdown( void );

uint64_t catbus_u64_get_origin_id( void );
const catbus_hash_t32* catbus_hp_get_tag_hashes( void );
bool catbus_b_query_self( catbus_query_t *query );
bool catbus_b_query_single( catbus_hash_t32 hash, catbus_query_t *tags );
bool catbus_b_query_tags( catbus_query_t *query, catbus_query_t *tags );
void catbus_v_get_query( catbus_query_t *query );

#define CATBUS_MAX_HASH_RESOLVER_LOOKUPS    4
#define CATBUS_HASH_LOOKUP_INTERVAL         2000 // ms
#define CATBUS_HASH_LOOKUP_TRIES            4

typedef struct  __attribute__((packed)){
    catbus_hash_t32 hash;
    ip_addr4_t host_ip;
    uint8_t tries;
} catbus_hash_lookup_t;

int8_t catbus_i8_get_string_for_hash( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN], ip_addr4_t *host_ip );

#endif




