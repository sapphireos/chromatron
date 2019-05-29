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

#ifndef _VM_CORE_H
#define _VM_CORE_H

#include <stdint.h>
#include "catbus_common.h"
#include "datetime_struct.h"
#include "target.h"

#define VM_ISA_VERSION              12

#define RETURN_VAL_ADDR             0
#define PIX_ARRAY_ADDR              1

#define FILE_MAGIC      0x20205846  // 'FX  '
#define PROGRAM_MAGIC   0x474f5250  // 'PROG'
#define CODE_MAGIC      0x45444f43  // 'CODE'
#define DATA_MAGIC      0x41544144  // 'DATA'
#define META_MAGIC      0x4154454d  // 'META'

#define DATA_LEN                    4

#define VM_STATUS_OK                    0
#define VM_STATUS_ERR_BAD_HASH          -1
#define VM_STATUS_ERR_BAD_FILE_MAGIC    -2
#define VM_STATUS_ERR_BAD_PROG_MAGIC    -3
#define VM_STATUS_ERR_INVALID_ISA       -4
#define VM_STATUS_ERR_BAD_CODE_MAGIC    -5
#define VM_STATUS_ERR_BAD_DATA_MAGIC    -6
#define VM_STATUS_ERR_BAD_FILE_HASH     -7
#define VM_STATUS_ERR_BAD_LENGTH        -8

#define VM_STATUS_ERR_MAX_CYCLES        -30

#define VM_STATUS_CODE_MISALIGN         -40
#define VM_STATUS_DATA_MISALIGN         -41
#define VM_STATUS_IMAGE_TOO_LARGE       -42
#define VM_STATUS_HEADER_MISALIGN       -43
#define VM_STATUS_READ_KEYS_MISALIGN    -44
#define VM_STATUS_WRITE_KEYS_MISALIGN   -45
#define VM_STATUS_PUBLISH_VARS_MISALIGN -46
#define VM_STATUS_PIXEL_MISALIGN        -47
#define VM_STATUS_LINK_MISALIGN         -48
#define VM_STATUS_DB_MISALIGN           -49
#define VM_STATUS_CRON_MISALIGN         -52

#define VM_STATUS_LOAD_ALLOC_FAIL       -50
#define VM_STATUS_SYNC_FAIL             -51



#define VM_STATUS_RESTRICTED_KEY        -70

#define VM_STATUS_ERROR                 -80

#define VM_STATUS_IMPROPER_YIELD        -96
#define VM_STATUS_INDEX_OUT_OF_BOUNDS   -97
#define VM_STATUS_CALL_DEPTH_EXCEEDED   -98
#define VM_STATUS_ASSERT                -99
#define VM_STATUS_TRAP                  -100
#define VM_STATUS_NOT_RUNNING           -127
#define VM_STATUS_HALT                  1
#define VM_STATUS_YIELDED               2
#define VM_STATUS_NO_THREADS            3
#define VM_STATUS_READY                 4
#define VM_STATUS_WAIT_SYNC             5


#define VM_LOAD_FLAGS_CHECK_HEADER      1


#define VM_SYS_CALL_TEST                0
#define VM_SYS_CALL_YIELD               1
#define VM_SYS_CALL_DELAY               2
#define VM_SYS_CALL_START_THREAD        10
#define VM_SYS_CALL_STOP_THREAD         11
#define VM_SYS_CALL_THREAD_RUNNING      12

#define VM_ARRAY_FUNC_LEN               0
#define VM_ARRAY_FUNC_MIN               1
#define VM_ARRAY_FUNC_MAX               2
#define VM_ARRAY_FUNC_AVG               3
#define VM_ARRAY_FUNC_SUM               4


typedef struct __attribute__((packed)){
    int8_t status;
    uint16_t loop_time;
    uint16_t thread_time;
    uint16_t max_cycles;
} vm_info_t;

// note this needs to pad to 32 bit alignment!
typedef struct __attribute__((packed)){
    uint32_t hash;
    uint16_t addr;
    uint8_t type;
    uint8_t padding[1];
} vm_publish_t;


typedef struct __attribute__((packed)){
    bool send;
    uint8_t padding[3];
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
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
    uint16_t data_len;
    uint16_t read_keys_len;     // length in BYTES, not number of objects!
    uint16_t write_keys_len;    // length in BYTES, not number of objects!
    uint16_t publish_len;       // length in BYTES, not number of objects!
    uint16_t pix_obj_len;       // length in BYTES, not number of objects!
    uint16_t link_len;          // length in BYTES, not number of objects!
    uint16_t db_len;            // length in BYTES, not number of objects!
    uint16_t cron_len;          // length in BYTES, not number of objects!
    uint16_t init_start;
    uint16_t loop_start;
    // variable length data:
    // - read keys
    // - write keys
    // - publish vars
    // - links
    // - db entries
    // - cron entries
    // - code stream
    // - data table
} vm_program_header_t;

typedef struct{
    uint16_t func_addr;
    uint16_t pc_offset;
    uint32_t delay_ticks;
} vm_thread_t;

typedef struct __attribute__((packed)){
    uint16_t addr;
    uint16_t length;
} vm_string_t;

typedef struct __attribute__((packed, aligned(4))){ // MUST be 32 bit aligned!
    uint16_t code_start;
    uint16_t data_start;
    uint16_t prog_size;
    uint16_t data_len;
    
    uint16_t data_count;
    uint16_t init_start;
    uint16_t loop_start;
    uint16_t frame_number;

    uint32_t program_name_hash;

    // MUST BE 32 bit aligned on ESP8266!
    uint64_t rng_seed;

    uint32_t tick_rate;

    vm_thread_t threads[VM_MAX_THREADS];
    uint32_t yield;

    int32_t current_thread;
    uint32_t max_cycles;

    uint32_t read_keys_count;
    uint32_t read_keys_start;

    uint32_t write_keys_count;
    uint32_t write_keys_start;

    uint32_t publish_count;
    uint32_t publish_start;

    uint32_t pix_obj_count;

    uint32_t link_count;
    uint32_t link_start;

    uint32_t db_count;
    uint32_t db_start;

    uint32_t cron_count;
    uint32_t cron_start;
    uint32_t last_cron;
} vm_state_t;

int8_t vm_i8_run(
    uint8_t *stream,
    uint16_t func_addr,
    uint16_t pc_offset,
    vm_state_t *state );

int8_t vm_i8_run_init(
    uint8_t *stream,
    vm_state_t *state );

int8_t vm_i8_run_loop(
    uint8_t *stream,
    vm_state_t *state );

int8_t vm_i8_run_threads(
    uint8_t *stream,
    vm_state_t *state );

int32_t vm_i32_get_data( 
    uint8_t *stream,
    vm_state_t *state,
    uint8_t addr );

void vm_v_get_data_multi( 
    uint8_t *stream,
    vm_state_t *state,
    uint8_t addr, 
    uint16_t len,
    int32_t *dest );

void vm_v_set_data( 
    uint8_t *stream,
    vm_state_t *state,
    uint8_t addr, 
    int32_t data );

int8_t vm_i8_load_program(
    uint8_t flags,
    uint8_t *stream,
    uint16_t len,
    vm_state_t *state );

void vm_v_init_db(
    uint8_t *stream,
    vm_state_t *state,
    uint8_t tag );

void vm_v_clear_db( uint8_t tag );

#endif
