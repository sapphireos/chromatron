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


#ifndef _MEM_H
#define _MEM_H

#include <inttypes.h>

#include "bool.h"


// #define ENABLE_EXTENDED_VERIFY

#define cnt_of_array( array ) ( sizeof( array ) / sizeof( array[0] ) )

#define MAX_MEM_HANDLES 128

#define MEM_DEFRAG_THRESHOLD    512

typedef int16_t mem_handle_t;

typedef uint8_t mem_type_t8;
#define MEM_TYPE_UNKNOWN            0
#define MEM_TYPE_LIST               1
#define MEM_TYPE_VM                 2
#define MEM_TYPE_NET                3
#define MEM_TYPE_MSG                4
#define MEM_TYPE_KVDB_ENTRY         5


typedef struct{
    uint16_t size;
    mem_handle_t handle;
    mem_type_t8 type;
    uint8_t padding_len;
    uint16_t header_padding;
    #ifdef ENABLE_RECORD_CREATOR
    uint16_t creator_address;
    #endif
} mem_block_header_t;

#define MEM_SIZE_DIRTY_MASK 0x8000

// memory run time data structure
// used for internal record keeping, and can also be accessed externally as a read only
typedef struct{
    uint16_t heap_size;
    uint16_t handles_used;
    uint16_t free_space;
    uint16_t used_space;
    uint16_t dirty_space;
    uint16_t data_space;
    uint16_t peak_usage;
} mem_rt_data_t;


#define ASSERT(expr) if( !(expr) ){  mem_assert( __FILE__, __LINE__, 0 ); }
#define ASSERT_MSG(expr, msg, ...) if( !(expr) ){  mem_assert(__FILE__,  __LINE__, msg, ##__VA_ARGS__ ); }

void mem_assert( const char* file, int line, const char *format, ... );

void mem2_v_init( uint8_t *_heap, uint16_t size );

mem_block_header_t mem2_h_get_header( uint16_t index );

bool mem2_b_verify_handle( mem_handle_t handle );
mem_handle_t mem2_h_alloc( uint16_t size );
mem_handle_t mem2_h_alloc2( uint16_t size, mem_type_t8 type );
mem_handle_t mem2_h_get_handle( uint8_t index, mem_type_t8 type );
#define MEM_ERR_INCORRECT_TYPE  -10
#define MEM_ERR_INVALID_HANDLE  -11
#define MEM_ERR_END_OF_LIST     -20

#ifdef ENABLE_EXTENDED_VERIFY
    void _mem2_v_free( mem_handle_t handle, const char* file, int line );
    uint16_t _mem2_u16_get_size( mem_handle_t handle, const char* file, int line );
    void *_mem2_vp_get_ptr( mem_handle_t handle, const char* file, int line );
    int8_t _mem2_i8_realloc( mem_handle_t handle, uint16_t size, const char* file, int line );

    #define mem2_v_free(handle)         _mem2_v_free(handle, __FILE__, __LINE__ )
    #define mem2_u16_get_size(handle)   _mem2_u16_get_size(handle, __FILE__, __LINE__ )
    #define mem2_vp_get_ptr(handle)     _mem2_vp_get_ptr(handle, __FILE__, __LINE__ )
    #define mem2_i8_realloc(handle, size)     _mem2_i8_realloc(handle, size, __FILE__, __LINE__ )

#else
    void mem2_v_free( mem_handle_t handle );
    uint16_t mem2_u16_get_size( mem_handle_t handle );
    void *mem2_vp_get_ptr( mem_handle_t handle );
    int8_t mem2_i8_realloc( mem_handle_t handle, uint16_t size );
#endif

void *mem2_vp_get_ptr_fast( mem_handle_t handle );

void mem2_v_check_canaries( void );
uint16_t mem2_u16_get_handles_used( void );
void mem2_v_get_rt_data( mem_rt_data_t *rt_data );
uint16_t mem2_u16_get_free( void );
uint16_t mem2_u16_get_dirty( void );
void mem2_v_collect_garbage( void );

#endif