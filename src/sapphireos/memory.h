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


#ifndef _MEMORY_H
#define _MEMORY_H

#include "system.h"
#include "memtypes.h"

//#define MAX_MEM_HANDLES         // defined in target.h

// #define MEM_MAX_STACK          // defined in target.h
#define MEM_STACK_GUARD_SIZE    8
#define MEM_STACK_HEAP_GUARD    8

// defragmenter will only run after the amount of dirty space exceeds this threshold
#define MEM_DEFRAG_THRESHOLD    128

// uncomment to make memory functions atomic
#define ENABLE_ATOMIC_MEMORY

#define ENABLE_EXTENDED_VERIFY

// extended verify is useless without asserts (and also won't compile)
#ifndef INCLUDE_ASSERTS
    #undef ENABLE_EXTENDED_VERIFY
#endif
//#define ENABLE_RECORD_CREATOR

#if (MAX_MEM_HANDLES < 256) && !defined(ALIGN32)
    typedef int8_t mem_handle_t;
#else
    typedef int16_t mem_handle_t;
#endif

typedef struct{
	uint16_t size;
	mem_handle_t handle;
    mem_type_t8 type;
    #ifdef ALIGN32
    uint8_t padding_len;
    uint16_t header_padding;
    #endif
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
    uint16_t peak_handles;
} mem_rt_data_t;

typedef struct __attribute__((packed)){
    uint16_t size;
    mem_type_t8 type;
} mem_info_t;

void mem2_v_init( void );

mem_block_header_t mem2_h_get_header( uint16_t index );

bool mem2_b_verify_handle( mem_handle_t handle );
mem_handle_t mem2_h_alloc( uint16_t size );
mem_handle_t mem2_h_alloc2( uint16_t size, mem_type_t8 type );
mem_handle_t mem2_h_get_handle( uint16_t index, mem_type_t8 type );
#define MEM_ERR_INCORRECT_TYPE  -10
#define MEM_ERR_INVALID_HANDLE  -11
#define MEM_ERR_END_OF_LIST     -20

#ifdef ENABLE_EXTENDED_VERIFY
    void _mem2_v_free( mem_handle_t handle, FLASH_STRING_T file, int line );
    uint16_t _mem2_u16_get_size( mem_handle_t handle, FLASH_STRING_T file, int line );
    void *_mem2_vp_get_ptr( mem_handle_t handle, FLASH_STRING_T file, int line );
    int8_t _mem2_i8_realloc( mem_handle_t handle, uint16_t size, FLASH_STRING_T file, int line );

    #define mem2_v_free(handle)         _mem2_v_free(handle, __SOURCE_FILE__, __LINE__ )
    #define mem2_u16_get_size(handle)   _mem2_u16_get_size(handle, __SOURCE_FILE__, __LINE__ )
    #define mem2_vp_get_ptr(handle)     _mem2_vp_get_ptr(handle, __SOURCE_FILE__, __LINE__ )
    #define mem2_i8_realloc(handle, size)     _mem2_i8_realloc(handle, size, __SOURCE_FILE__, __LINE__ )

#else
    void mem2_v_free( mem_handle_t handle );
    uint16_t mem2_u16_get_size( mem_handle_t handle );
    void *mem2_vp_get_ptr( mem_handle_t handle );
    int8_t mem2_i8_realloc( mem_handle_t handle, uint16_t size );
#endif

void *mem2_vp_get_ptr_fast( mem_handle_t handle );

void mem2_v_check_canaries( void );
uint16_t mem_u16_get_stack_usage( void );
uint16_t mem2_u16_get_handles_used( void );
void mem2_v_get_rt_data( mem_rt_data_t *rt_data );
uint16_t mem2_u16_get_free( void );
uint16_t mem2_u16_get_dirty( void );
void mem2_v_collect_garbage( void );
uint16_t mem2_u16_stack_count( void );

#endif
