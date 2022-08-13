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

#ifndef __KEYVALUE_H
#define __KEYVALUE_H

#include "system.h"
#include "types.h"
#include "ip.h"
#include "ntp.h"
#include "threading.h"
#include "sockets.h"
#include "catbus.h"

#define KV_PERSIST_VERSION          4

#define KV_NAME_LEN                 CATBUS_STRING_LEN


#ifndef ENABLE_NETWORK
    #undef ENABLE_KV_NOTIFICATIONS
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_META
#else
    #define KV_SECTION_META              __attribute__ ((section (".kv_meta"), used))
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define SERVICE_SECTION
#else
    #define SERVICE_SECTION              __attribute__ ((section (".service"), used))
#endif

typedef struct{
    char svc_name[32];
} kv_svc_name_t;


typedef uint8_t kv_op_t8;
#define KV_OP_SET                   1
#define KV_OP_GET                   2

typedef uint8_t kv_flags_t8;
#define KV_FLAGS_READ_ONLY          0x0001
#define KV_FLAGS_PERSIST            0x0004
#define KV_FLAGS_DYNAMIC            0x0008

#define KV_ARRAY_LEN(len) ( len - 1 )

// Error codes
#define KV_ERR_STATUS_OK                        0
#define KV_ERR_STATUS_NOT_FOUND                 -1
#define KV_ERR_STATUS_READONLY                  -2
#define KV_ERR_STATUS_INVALID_TYPE              -3
#define KV_ERR_STATUS_OUTPUT_BUF_TOO_SMALL      -4
#define KV_ERR_STATUS_INPUT_BUF_TOO_SMALL       -5
#define KV_ERR_STATUS_TYPE_MISMATCH             -6
#define KV_ERR_STATUS_SAFE_MODE                 -7
#define KV_ERR_STATUS_PARAMETER_NOT_SET         -8
#define KV_ERR_STATUS_CANNOT_CONVERT_TYPES      -9
#define KV_ERR_STATUS_OUT_OF_BOUNDS            -10

typedef struct  __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t reserved[3];
} kv_persist_file_header_t;
#define KV_PERSIST_MAGIC         0x0050564b // 'KVP' with a leading 0

typedef int8_t ( *kv_handler_t )(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len );
    
typedef struct  __attribute__((packed, aligned(4))){
    catbus_type_t8 type;
    uint8_t array_len;
    kv_flags_t8 flags;
    void *ptr;
    kv_handler_t handler;
    char name[CATBUS_STRING_LEN];
    catbus_hash_t32 hash;
    uint8_t padding;
} kv_meta_t;

typedef struct  __attribute__((packed)){
    catbus_hash_t32 hash;
    uint16_t index;
} kv_hash_index_t;




// prototypes:

void kv_v_init( void );

int16_t kv_i16_len( catbus_hash_t32 hash );
catbus_type_t8 kv_i8_type( catbus_hash_t32 hash );
void kv_v_reset_cache( void );

int8_t kv_i8_persist( catbus_hash_t32 hash );
    
uint16_t kv_u16_get_size_meta( kv_meta_t *meta );

int8_t kv_i8_set(
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len );

int8_t kv_i8_array_set(
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    const void *data,
    uint16_t len );

int8_t kv_i8_get(
    catbus_hash_t32 hash,
    void *data,
    uint16_t max_len );

int8_t kv_i8_array_get(
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t max_len );

bool kv_b_get_boolean( catbus_hash_t32 hash );

int16_t kv_i16_search_hash( catbus_hash_t32 hash );
int8_t kv_i8_get_name( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN] );
uint16_t kv_u16_count( void );
int8_t kv_i8_lookup_index( uint16_t index, kv_meta_t *meta );
int8_t kv_i8_lookup_index_with_name( uint16_t index, kv_meta_t *meta );
int8_t kv_i8_lookup_hash(
    catbus_hash_t32 hash,
    kv_meta_t *meta );
int8_t kv_i8_lookup_hash_with_name(
    catbus_hash_t32 hash,
    kv_meta_t *meta );
int8_t kv_i8_get_catbus_meta( catbus_hash_t32 hash, catbus_meta_t *meta );

int8_t kv_i8_internal_get(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t max_len );

// dynamic keys
#define KV_META_FLAGS_GET_NAME      0x01

uint8_t kv_u8_get_dynamic_count( void );
void kv_v_shutdown( void );

extern void kv_v_notify_hash_set( catbus_hash_t32 hash ) __attribute__((weak));

#endif
