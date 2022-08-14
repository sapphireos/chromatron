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

#ifndef __MSGFLOW_H
#define __MSGFLOW_H

#include "system.h"
#include "memory.h"
#include "threading.h"
#include "catbus_common.h"
#include "udp.h"

#define MSGFLOW_TIMEOUT                 16
#define MSGFLOW_KEEPALIVE               4

#ifdef ENABLE_MSGFLOW

typedef struct __attribute__((packed, aligned(4))){
    uint8_t type;
    uint8_t flags;
    uint16_t reserved;
} msgflow_header_t;

#define MSGFLOW_FLAGS_VERSION           1
#define MSGFLOW_FLAGS_VERSION_MASK      0x0F

#define MSGFLOW_CODE_INVALID            0
#define MSGFLOW_CODE_NONE               1 // just hope it gets there
#define MSGFLOW_CODE_ARQ                2 // ARQ
#define MSGFLOW_CODE_TRIPLE             3 // 3x repetition
#define MSGFLOW_CODE_SMALLBUF           4 // rolling buffer to full packet size
#define MSGFLOW_CODE_1_OF_4_PARITY      5 // corrects 1 missing blocks of 4.  3 data, 1 parity
#define MSGFLOW_CODE_2_OF_7_PARITY      6 // corrects 2 missing blocks of 7.  4 data, 3 parity

#define MSGFLOW_CODE_ANY                0xff // special value to allow the driver to select the code

#define MSGFLOW_CODE_DEFAULT            MSGFLOW_CODE_ARQ


#define MSGFLOW_ARQ_TRIES               8

typedef struct __attribute__((packed)){
    uint16_t max_data_len; // set maximum data for each message.  useful to limit mem usage for parity.
    uint8_t code;
    uint8_t flags;
    uint64_t device_id;
} msgflow_msg_reset_t;
#define MSGFLOW_TYPE_RESET              2

typedef struct __attribute__((packed)){
    uint8_t code;
    uint8_t reserved[3];
} msgflow_msg_ready_t;
#define MSGFLOW_TYPE_READY              3

typedef struct __attribute__((packed)){
    uint64_t sequence;
    uint8_t reserved[12];
} msgflow_msg_status_t;
#define MSGFLOW_TYPE_STATUS             4

typedef struct __attribute__((packed)){
    uint64_t sequence;
    // data follows
} msgflow_msg_data_t;
#define MSGFLOW_TYPE_DATA               5

// empty messages
#define MSGFLOW_TYPE_STOP               6
#define MSGFLOW_TYPE_QUERY_CODEBOOK     7

typedef struct __attribute__((packed)){
    uint8_t codebook[8]; // list of supported codes
} msgflow_msg_codebook_t;
#define MSGFLOW_TYPE_CODEBOOK           8

#define MSGFLOW_MAX_LEN                 ( UDP_MAX_LEN - ( sizeof(msgflow_header_t) + sizeof(msgflow_msg_data_t) ) )


typedef thread_t msgflow_t;



void msgflow_v_init( void );

msgflow_t msgflow_m_listen( catbus_hash_t32 service, uint8_t code, uint16_t max_msg_size );
bool msgflow_b_connected( msgflow_t msgflow );
bool msgflow_b_send( msgflow_t msgflow, void *data, uint16_t len );
void msgflow_v_close( msgflow_t msgflow );

void msgflow_v_process_timeouts( void );

#endif

#endif

