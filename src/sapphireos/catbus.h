/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "ntp.h"
#include "list.h"

#define CATBUS_DISCOVERY_PORT               44632
#define CATBUS_VERSION                      1
#define CATBUS_MEOW                         0x574f454d // 'MEOW'

#define CATBUS_ANNOUNCE_INTERVAL            8
#define CATBUS_MAX_RECEIVE_LINKS            16
#define CATBUS_MAX_SEND_LINKS               16
#define CATBUS_MAX_FILE_SESSIONS            4
#define CATBUS_LINK_TIMEOUT                 64            


typedef struct __attribute__((packed)){
    catbus_meta_t meta;
    // variable length data follows
    uint8_t data; // convenient dereferencing point to first byte
} catbus_data_t;

typedef int16_t catbus_link_t;

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
#define CATBUS_MSG_LINK_GROUP_OFFSET            40
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

#define CATBUS_ERROR_FILE_NOT_FOUND             0x0101
#define CATBUS_ERROR_FILESYSTEM_FULL            0x0102
#define CATBUS_ERROR_FILESYSTEM_BUSY            0x0103
#define CATBUS_ERROR_INVALID_FILE_SESSION       0x0104

#define CATBUS_ERROR_LINK_EOF                   0x0201


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


// LINK
#define CATBUS_MSG_DATA_FLAG_TIME_SYNC          0x01

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    uint16_t data_port;
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
    catbus_query_t query;
    catbus_hash_t32 tag;
} catbus_msg_link_t;
#define CATBUS_MSG_TYPE_LINK                    ( 1 + CATBUS_MSG_LINK_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    ntp_ts_t ntp_timestamp;
    catbus_query_t source_query;
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
    uint16_t sequence;
    catbus_data_t data;
} catbus_msg_link_data_t;
#define CATBUS_MSG_TYPE_LINK_DATA               ( 2 + CATBUS_MSG_LINK_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint16_t index;
} catbus_msg_link_get_t;
#define CATBUS_MSG_TYPE_LINK_GET                ( 3 + CATBUS_MSG_LINK_GROUP_OFFSET )

// same message as catbus_msg_link_t
#define CATBUS_MSG_TYPE_LINK_META               ( 4 + CATBUS_MSG_LINK_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    catbus_hash_t32 tag;
} catbus_msg_link_delete_t;
#define CATBUS_MSG_TYPE_LINK_DELETE             ( 5 + CATBUS_MSG_LINK_GROUP_OFFSET )

typedef struct __attribute__((packed)){
    catbus_header_t header;
    uint8_t flags;
    catbus_string_t source_key;
    catbus_string_t dest_key;
    catbus_string_t query[CATBUS_QUERY_LEN];
    catbus_string_t tag;
} catbus_msg_link_add_t;
#define CATBUS_MSG_TYPE_LINK_ADD                ( 6 + CATBUS_MSG_LINK_GROUP_OFFSET )

// empty message
#define CATBUS_MSG_TYPE_LINK_OK                 ( 7 + CATBUS_MSG_LINK_GROUP_OFFSET )



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

catbus_link_t catbus_l_send( catbus_hash_t32 source_hash, 
                             catbus_hash_t32 dest_hash, 
                             catbus_query_t *dest_query,
                             catbus_hash_t32 tag,
                             uint8_t flags );
catbus_link_t catbus_l_recv( catbus_hash_t32 dest_hash, 
                             catbus_hash_t32 source_hash, 
                             catbus_query_t *source_query,
                             catbus_hash_t32 tag,
                             uint8_t flags );

void catbus_v_purge_links( catbus_hash_t32 tag );

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

int8_t catbus_i8_publish( catbus_hash_t32 hash );

void catbus_v_set_options( uint32_t options );
#define CATBUS_OPTION_LINK_DISABLE      0x00000001

void catbus_v_shutdown( void );

#endif




