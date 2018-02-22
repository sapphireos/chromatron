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

#ifndef _AUTOMATON_H
#define _AUTOMATON_H

#include "target.h"

#ifdef ENABLE_AUTOMATON

#include "catbus.h"

#define AUTOMATON_KV_TAG            0x40

#define AUTOMATON_REG_COUNT         32
#define AUTOMATON_MAX_VARS          16
#define AUTOMATON_CODE_LEN          255

#define AUTOMATON_FILE_MAGIC        0x4F545541  // 'AUTO'
#define AUTOMATON_RULE_MAGIC        0x454C5552  // 'RULE'
#define AUTOMATON_VERSION           1

#define AUTOMATON_STATUS_RUNNING            0
#define AUTOMATON_STATUS_STOPPED            -1
#define AUTOMATON_STATUS_LOAD_ERROR         -2
#define AUTOMATON_STATUS_DATA_OVERFLOW      -3
#define AUTOMATON_STATUS_DB_ADD_FAIL        -4
#define AUTOMATON_STATUS_CODE_OVERFLOW      -5
#define AUTOMATON_STATUS_BAD_VERSION        -6
#define AUTOMATON_STATUS_TRIGGER_INDEX_FAIL -7
#define AUTOMATON_STATUS_FILE_NOT_FOUND     -8
#define AUTOMATON_STATUS_ERROR              -9


typedef struct __attribute__((packed)){
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
    catbus_query_t query;
} automaton_link_t;

typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    uint16_t condition_offset;
    uint8_t status;
} automaton_trigger_index_t;

typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    char name[KV_NAME_LEN];
} automaton_var_t;

typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    uint8_t addr;
} automaton_kv_load_t;


typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t condition_data_len;
    uint8_t condition_kv_len;
    uint8_t condition_code_len;
    uint8_t action_data_len;
    uint8_t action_kv_len;
    uint8_t action_code_len;

    // variable length arrays follow in this order:
    // uint8_t condition_data[];
    // uint8_t condition_kv[];
    // uint8_t condition_code[];
    // uint8_t action_data[];
    // uint8_t action_kv[];
    // uint8_t action_code[];
} automaton_rule_t;

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t var_len;
    uint8_t send_len;
    uint8_t recv_len;
    uint8_t trigger_index_len;
    uint8_t rules_len;

    // variable length arrays follow in this order:
    // automaton_var_t kv_vars[];
    // automaton_link_t send_links[];
    // automaton_link_t receive_links[];
    // automaton_trigger_index_t trigger_index[];
    // automaton_rule_t rules[];

} automaton_file_t;


void auto_v_init( void );



#endif
#endif