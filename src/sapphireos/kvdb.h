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

#ifndef _KVDB_H
#define _KVDB_H

#include <inttypes.h>
#include "catbus_common.h"
#include "catbus_types.h"
#include "kvdb_config.h"

#define KVDB_MAX_DATA 	CATBUS_MAX_DATA 

typedef void ( *kvdb_notifier_t )(
    catbus_hash_t32 hash,
    catbus_type_t8 type,
    const void *data );

void kvdb_v_init( void );
uint16_t kvdb_u16_count( void );
uint16_t kvdb_u16_db_size( void );

int8_t kvdb_i8_add( 
    catbus_hash_t32 hash, 
    catbus_type_t8 type,
    uint16_t count,
    const void *data,
    uint16_t len );

void kvdb_v_set_name( char name[CATBUS_STRING_LEN] );
#ifdef PGM_P
void kvdb_v_set_name_P( PGM_P name );
#endif
void kvdb_v_set_tag( catbus_hash_t32 hash, uint8_t tag );
void kvdb_v_clear_tag( catbus_hash_t32 hash, uint8_t tag );
void kvdb_v_set_notifier( catbus_hash_t32 hash, kvdb_notifier_t notifier );

int8_t kvdb_i8_set( catbus_hash_t32 hash, catbus_type_t8 type, const void *data, uint16_t len );
int8_t kvdb_i8_get( catbus_hash_t32 hash, catbus_type_t8 type, void *data, uint16_t max_len );
int8_t kvdb_i8_array_set( catbus_hash_t32 hash, catbus_type_t8 type, uint16_t index, const void *data, uint16_t len );
int8_t kvdb_i8_array_get( catbus_hash_t32 hash, catbus_type_t8 type, uint16_t index, void *data, uint16_t max_len );

int8_t kvdb_i8_notify( catbus_hash_t32 hash );
int8_t kvdb_i8_get_meta( catbus_hash_t32 hash, catbus_meta_t *meta );
void *kvdb_vp_get_ptr( catbus_hash_t32 hash );

void kvdb_v_delete( catbus_hash_t32 hash );

#ifdef KVDB_ENABLE_NAME_LOOKUP
int8_t kvdb_i8_lookup_name( catbus_hash_t32 hash, char name[CATBUS_STRING_LEN] );
#endif

catbus_hash_t32 kvdb_h_get_hash_for_index( uint16_t index );
int16_t kvdb_i16_get_index_for_hash( catbus_hash_t32 hash );

// uint16_t kvdb_u16_read( catbus_hash_t32 hash );
// uint8_t kvdb_u8_read( catbus_hash_t32 hash );
// int8_t kvdb_i8_read( catbus_hash_t32 hash );
// int16_t kvdb_i16_read( catbus_hash_t32 hash );
// int32_t kvdb_i32_read( catbus_hash_t32 hash );
// bool kvdb_b_read( catbus_hash_t32 hash );

extern void kvdb_v_notify_set( catbus_hash_t32 hash, catbus_meta_t *meta, const void *data ) __attribute__((weak));

#define KVDB_STATUS_OK                  0
#define KVDB_STATUS_NOT_FOUND           -1
#define KVDB_STATUS_NOT_ENOUGH_SPACE    -2
#define KVDB_STATUS_INVALID_HASH        -3
#define KVDB_STATUS_HASH_CONFLICT       -4
#define KVDB_STATUS_DATA_TOO_LARGE      -5
#define KVDB_STATUS_LENGTH_MISMATCH     -6


#endif