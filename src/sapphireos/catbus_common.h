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

#ifndef __CATBUS_COMMON_H
#define __CATBUS_COMMON_H

#include <inttypes.h>

#include "catbus_types.h"

typedef uint8_t catbus_flags_t8;
#define CATBUS_FLAGS_READ_ONLY              0x01
#define CATBUS_FLAGS_PERSIST                0x04
#define CATBUS_FLAGS_DYNAMIC                0x08

#define CATBUS_MAX_DATA                     512
#define CATBUS_STRING_LEN                   32
#define CATBUS_QUERY_LEN                    8

typedef struct __attribute__((packed)){
    char str[CATBUS_STRING_LEN];
} catbus_string_t;

typedef uint32_t catbus_hash_t32;

typedef struct __attribute__((packed)){
    catbus_hash_t32 tags[CATBUS_QUERY_LEN];
} catbus_query_t;


typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    uint8_t count;
    catbus_flags_t8 flags;
    uint8_t reserved;
} catbus_meta_t;

typedef struct __attribute__((packed)){
    int32_t size;
    uint8_t flags;
    uint8_t reserved[3];
    char filename[CATBUS_STRING_LEN];
} catbus_file_meta_t;


void type_v_convert( 
    catbus_type_t8 dest_type,
    uint8_t dest_count,
    void *dest_data,
    catbus_type_t8 src_type,
    uint8_t src_count,
    void *src_data );

#endif
