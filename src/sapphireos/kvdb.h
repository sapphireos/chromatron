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

#ifndef _KVDB_H
#define _KVDB_H

#include <inttypes.h>
#include "catbus_common.h"
#include "catbus_types.h"
#include "kvdb_config.h"

void kvdb_v_init( void );
uint16_t kvdb_u16_count( void );
uint16_t kvdb_u16_db_size( void );
int8_t kvdb_i8_add( catbus_hash_t32 hash, int32_t data, uint8_t tag, char name[CATBUS_STRING_LEN] );
int8_t kvdb_i8_set( catbus_hash_t32 hash, int32_t data );
int8_t kvdb_i8_get( catbus_hash_t32 hash, int32_t *data );
int8_t kvdb_i8_get_meta( catbus_hash_t32 hash, catbus_meta_t *meta );
void kvdb_v_delete( catbus_hash_t32 hash );
void kvdb_v_delete_tag( uint8_t tag );
int8_t kvdb_i8_publish( catbus_hash_t32 hash );
#ifdef KVDB_ENABLE_NAME_LOOKUP
int8_t kvdb_i8_lookup_name( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN] );
#endif
catbus_hash_t32 kvdb_h_get_hash_for_index( uint16_t index );
int16_t kvdb_i16_get_index_for_hash( catbus_hash_t32 hash );

uint16_t kvdb_u16_read( catbus_hash_t32 hash );
uint8_t kvdb_u8_read( catbus_hash_t32 hash );
int8_t kvdb_i8_read( catbus_hash_t32 hash );
int16_t kvdb_i16_read( catbus_hash_t32 hash );
int32_t kvdb_i32_read( catbus_hash_t32 hash );
bool kvdb_b_read( catbus_hash_t32 hash );

extern void kvdb_v_notify_set( catbus_hash_t32 hash, catbus_meta_t *meta, void *data ) __attribute__((weak));
extern int8_t kvdb_i8_handle_publish( catbus_hash_t32 hash ) __attribute__((weak));

#define KVDB_STATUS_OK                  0
#define KVDB_STATUS_NOT_FOUND           -1
#define KVDB_STATUS_NOT_ENOUGH_SPACE    -2
#define KVDB_STATUS_INVALID_HASH        -3

#endif