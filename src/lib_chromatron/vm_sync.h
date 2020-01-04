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

#ifndef _VM_SYNC_H_
#define _VM_SYNC_H_

#include "target.h"

#ifdef ENABLE_TIME_SYNC

#include "vm_wifi_cmd.h"

#define SYNC_SERVER_PORT                	32038

#define SYNC_PROTOCOL_MAGIC             	0x434e5953 // 'SYNC' in ASCII
#define SYNC_PROTOCOL_VERSION           	2

#define SYNC_MASTER_TIMEOUT                 32000 // in milliseconds
#define SYNC_RATE                           8000

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint32_t sync_group_hash;
} vm_sync_msg_header_t;

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
    uint64_t uptime;
    uint32_t program_name_hash;
    uint16_t frame_number;
    uint16_t data_len;
    uint64_t rng_seed;
    uint32_t net_time;
} vm_sync_msg_sync_0_t;
#define VM_SYNC_MSG_SYNC_0                      1

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
    uint16_t frame_number;
    uint16_t offset;
} vm_sync_msg_sync_n_t;
#define VM_SYNC_MSG_SYNC_N                      2

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
} vm_sync_msg_sync_req_t;
#define VM_SYNC_MSG_SYNC_REQ                    3

typedef struct __attribute__((packed)){
    vm_sync_msg_header_t header;
} vm_sync_msg_shutdown_t;
#define VM_SYNC_MSG_SHUTDOWN                    10


uint32_t vm_sync_u32_get_sync_group_hash( void );

void vm_sync_v_init( void );
void vm_sync_v_trigger( void );
void vm_sync_v_frame_trigger( void );

bool vm_sync_b_is_master( void );
bool vm_sync_b_is_slave( void );
bool vm_sync_b_is_slave_synced( void );
bool vm_sync_b_is_synced( void );

#endif

#endif