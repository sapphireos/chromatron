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

#ifndef _VM_CORE_H
#define _VM_CORE_H

#include <stdint.h>
#include "catbus_common.h"
#include "catbus_link.h"
#include "datetime_struct.h"
#include "target.h"
#include "keyvalue.h"

#define VM_ISA_VERSION              13

#define VM_MAX_RUN_TIME             500 // milliseconds

#define RETURN_VAL_ADDR             0
#define PIX_ARRAY_ADDR              1

#define FILE_MAGIC      0x20205846  // 'FX  '
#define PROGRAM_MAGIC   0x474f5250  // 'PROG'
#define CODE_MAGIC      0x45444f43  // 'CODE'
#define POOL_MAGIC      0x4C4F4F50  // 'POOL'
#define META_MAGIC      0x4154454d  // 'META'

#define DATA_LEN                    4

#define VM_STATUS_OK                        0
#define VM_STATUS_ERR_BAD_HASH              -1
#define VM_STATUS_ERR_BAD_FILE_MAGIC        -2
#define VM_STATUS_ERR_BAD_PROG_MAGIC        -3
#define VM_STATUS_ERR_INVALID_ISA           -4
#define VM_STATUS_ERR_BAD_CODE_MAGIC        -5
#define VM_STATUS_ERR_BAD_POOL_MAGIC        -6
#define VM_STATUS_ERR_BAD_FILE_HASH         -7
#define VM_STATUS_ERR_BAD_LENGTH            -8
#define VM_STATUS_FX_FILE_NOT_FOUND         -9
#define VM_STATUS_ERR_BAD_FILE_READ         -10
#define VM_STATUS_ERR_FUNC_NOT_FOUND        -11
#define VM_STATUS_ERR_LOCAL_OUT_OF_BOUNDS   -12
#define VM_STATUS_ERR_BAD_META_MAGIC        -13

#define VM_STATUS_ERR_MAX_CYCLES            -30

#define VM_STATUS_CODE_MISALIGN             -40
#define VM_STATUS_DATA_MISALIGN             -41
#define VM_STATUS_IMAGE_TOO_LARGE           -42
#define VM_STATUS_HEADER_MISALIGN           -43
// #define VM_STATUS_READ_KEYS_MISALIGN        -44
// #define VM_STATUS_WRITE_KEYS_MISALIGN       -45
#define VM_STATUS_PUBLISH_VARS_MISALIGN     -46
#define VM_STATUS_PIXEL_MISALIGN            -47
#define VM_STATUS_LINK_MISALIGN             -48
#define VM_STATUS_DB_MISALIGN               -49
#define VM_STATUS_LOAD_ALLOC_FAIL           -50
#define VM_STATUS_SYNC_FAIL                 -51
#define VM_STATUS_CRON_MISALIGN             -52
#define VM_STATUS_STREAM_MISALIGN           -53
#define VM_STATUS_POOL_MISALIGN             -54
#define VM_STATUS_BAD_STORAGE_POOL          -55
#define VM_STATUS_INVALID_FUNC_REF          -56
#define VM_STATUS_BAD_PUBLISH_ADDR          -57
#define VM_STATUS_BAD_CONTEXT_SIZE          -58


#define VM_STATUS_RESTRICTED_KEY            -70

#define VM_STATUS_ERROR                     -80

#define VM_STATUS_IMPROPER_YIELD            -96
#define VM_STATUS_INDEX_OUT_OF_BOUNDS       -97
#define VM_STATUS_CALL_DEPTH_EXCEEDED       -98
#define VM_STATUS_ASSERT                    -99
#define VM_STATUS_TRAP                      -100
#define VM_STATUS_COMM_ERROR                -126
#define VM_STATUS_NOT_RUNNING               -127
#define VM_STATUS_HALT                      1
#define VM_STATUS_YIELDED                   2
#define VM_STATUS_DID_NOT_RUN               3
#define VM_STATUS_READY                     4


#ifndef VM_MAX_LINKS
    #define VM_MAX_LINKS 16
#endif


// note this needs to pad to 32 bit alignment!
typedef struct __attribute__((packed)){
    uint32_t hash;
    uint16_t addr;
    uint8_t type;
    kv_flags_t8 flags;
} vm_publish_t;

typedef struct __attribute__((packed)){
    uint16_t addr;
    uint16_t frame_size;
    uint16_t context_size;
    uint16_t padding;
} function_info_t;

typedef struct __attribute__((packed)){
    link_mode_t8 mode;
    link_aggregation_t8 aggregation;
    link_rate_t16 rate;
    catbus_hash_t32 source_key;
    catbus_hash_t32 dest_key;
    catbus_hash_t32 tag;
    catbus_query_t query;
} link_t;

typedef struct __attribute__((packed)){
    uint16_t func_addr;
    int8_t seconds;
    int8_t minutes;
    int8_t hours;
    int8_t day_of_month;
    int8_t day_of_week;
    int8_t month;
    bool run;
    uint8_t padding[3];
} cron_t;

typedef struct __attribute__((packed)){
    uint32_t file_magic;
    uint32_t prog_magic;
    uint16_t isa_version;
    uint32_t program_name_hash;
    uint16_t code_len;
    uint16_t func_info_len;
    uint16_t local_data_len;
    uint16_t global_data_len;
    uint16_t max_context_len;
    uint16_t padding;
    uint16_t constant_len;      // length in BYTES, not number of objects!
    uint16_t stringlit_len;     // length in BYTES, not number of objects!
    uint16_t publish_len;       // length in BYTES, not number of objects!
    uint16_t link_len;          // length in BYTES, not number of objects!
    uint16_t db_len;            // length in BYTES, not number of objects!
    uint16_t cron_len;          // length in BYTES, not number of objects!
    uint16_t pix_obj_len;       // length in BYTES, not number of objects!
    uint16_t init_start;
    uint16_t loop_start;
    // variable length data:
    // - init data
    // - constant pool
    // - string literal pool (placed after constant pool)
    // - read keys
    // - write keys
    // - publish vars
    // - links
    // - db entries
    // - cron entries
    // - code stream
    // - data table
} vm_program_header_t;

typedef struct __attribute__((packed)){
    uint16_t func_addr;
    uint16_t pc_offset;
    uint64_t tick;
} vm_thread_t;

typedef struct __attribute__((packed)){
    uint16_t addr;
    uint16_t length;
} vm_string_t;



#define POOL_GLOBAL             0
#define POOL_PIXEL_ARRAY        1
#define POOL_STRING_LITERALS    2
#define POOL_FUNCTIONS          3
#define POOL_LOCAL              4

/*

Changes for reference and pixel lookup:


Reference is currently 16 bits pool and 16 bits address.
There is no way we'll use 16 bits of pool space.

Change to an 8 bit pool, 16 bits of address, and 8 bits 
of index.

The index can mean whatever the selected pool needs it to be.
Most pools will be 0.
The pixel pool will use it for attribute selection.


Similar story for pixel lookup.
Right now, pixel lookup just loads a 32 bit index (for the master
array).


Pixel lookup should load an encoded value with the pixel index
in 16 bits, an 8 bit attribute selector, and 8 bits of reserved
for later use.

It's similar to the ref, but still distince: a Ref cannot
provide an index into the pixel array.

The pixel lookup index on the other hand cannot specify any object
other than the master pixel array, but it can index within it
and also select an attribute.


*/


typedef struct __attribute__((packed)){
    uint16_t pool;
    uint16_t addr;
} vm_packed_reference_t;

typedef union{
    vm_packed_reference_t ref;
    uint32_t n;
} vm_reference_t;


typedef struct __attribute__((packed, aligned(4))){ // MUST be 32 bit aligned!
    uint16_t vm_id;
    uint16_t code_start;

    uint16_t func_info_start;
    uint16_t func_info_len;

    uint16_t global_data_start;
    uint16_t local_data_start;

    uint16_t local_data_count;
    uint16_t local_data_len;

    uint16_t global_data_count;
    uint16_t global_data_len;
    
    uint16_t max_thread_context_size; // maximum per-thread context size
    uint16_t total_thread_context_size; // total context size for ALL threads

    uint16_t thread_context_start;
    uint16_t prog_size;

    uint16_t pool_start;
    uint16_t pool_len;

    uint16_t string_start;
    uint16_t string_len;

    uint16_t init_start;
    uint16_t loop_start;

    uint64_t tick;
    
    uint32_t last_elapsed_us;
    
    uint64_t loop_tick;
    
    uint32_t frame_number;

    int32_t return_val;

    uint32_t program_name_hash;

    // MUST BE 32 bit aligned on ESP8266!
    uint64_t rng_seed;

    vm_thread_t threads[VM_MAX_THREADS];
    
    // uint32_t yield;

    int32_t current_thread;
    
    uint32_t max_cycles;

    uint16_t publish_count;
    uint16_t publish_start;

    uint16_t link_count;
    uint16_t link_start;

    #ifdef ENABLE_CATBUS_LINK
    link_handle_t links[VM_MAX_LINKS];
    #endif

    uint16_t db_count;
    uint16_t db_start;

    uint16_t cron_count;
    // uint16_t cron_start;

    uint16_t last_cron;
    // uint16_t padding;

    uint16_t pix_obj_start;
    uint16_t pix_obj_count;
} vm_state_t;

int8_t vm_i8_run(
    uint8_t *stream,
    uint16_t func_addr,
    uint16_t pc_offset,
    vm_state_t *state );

int8_t vm_i8_run_tick(
    uint8_t *stream,
    vm_state_t *state,
    int32_t delta_ticks );

int8_t vm_i8_run_init(
    uint8_t *stream,
    vm_state_t *state );

int8_t vm_i8_run_loop(
    uint8_t *stream,
    vm_state_t *state );

uint64_t vm_u64_get_next_tick(
    uint8_t *stream,
    vm_state_t *state );

// int32_t vm_i32_get_data( 
//     uint8_t *stream,
//     vm_state_t *state,
//     uint16_t addr );

// void vm_v_get_data_multi( 
//     uint8_t *stream,
//     vm_state_t *state,
//     uint16_t addr, 
//     uint16_t len,
//     int32_t *dest );

int32_t* vm_i32p_get_data_ptr( 
    uint8_t *stream,
    vm_state_t *state );

uint16_t vm_u16_get_data_len( vm_state_t *state );

// void vm_v_set_data( 
//     uint8_t *stream,
//     vm_state_t *state,
//     uint16_t addr, 
//     int32_t data );

int8_t vm_i8_check_header( vm_program_header_t *prog_header );

int8_t vm_i8_load_program(
    uint8_t vm_id, 
    char *program_fname, 
    mem_handle_t *handle,
    vm_state_t *state );

/*void vm_v_init_db(
    uint8_t *stream,
    vm_state_t *state,
    uint8_t tag );
*/
// void vm_v_clear_db( uint8_t tag );

#endif
