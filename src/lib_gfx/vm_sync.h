// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#ifndef _VM_SYNC_H_
#define _VM_SYNC_H_

#include "target.h"

#ifdef ENABLE_TIME_SYNC

#define SYNC_PROTOCOL_MAGIC             	0x434e5953 // 'SYNC' in ASCII
#define SYNC_PROTOCOL_VERSION           	3

#define SYNC_SERVICE                        __KV__vmsync

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t padding;
    uint32_t sync_group_hash;
} vm_sync_msg_header_t;

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
    uint32_t program_name_hash;
    uint64_t tick;
    uint64_t rng_seed;
    uint32_t net_time;
    uint16_t data_len;
} vm_sync_msg_sync_t;
#define VM_SYNC_MSG_SYNC                        1

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
    bool request_data;
} vm_sync_msg_sync_req_t;
#define VM_SYNC_MSG_SYNC_REQ                    2

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
    uint64_t tick;
    uint16_t offset;
    uint16_t padding;
    uint8_t data; // first data byte
} vm_sync_msg_data_t;
#define VM_SYNC_MSG_DATA                        3
#define VM_SYNC_MAX_DATA_LEN                    512

uint32_t vm_sync_u32_get_sync_group_hash( void );

void vm_sync_v_init( void );

bool vm_sync_b_is_leader( void );
bool vm_sync_b_is_follower( void );
bool vm_sync_b_in_progress( void );


#endif

#endif
