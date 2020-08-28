/*
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
 */

#ifndef __ELECTION_H
#define __ELECTION_H

#include "sockets.h"

#define ELECTION_PORT               32036
#define ELECTION_MAGIC              0x45544f56 // 'VOTE'
#define ELECTION_VERSION            2

#define FOLLOWER_TIMEOUT            64
#define FOLLOWER_QUERY_TIMEOUT      8
#define CANDIDATE_TIMEOUT           6
#define IDLE_TIMEOUT                4

#define ELECTION_PRIORITY_FOLLOWER_ONLY    0

#define ELECTION_HDR_FLAGS_QUERY        0x01

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t count;
    uint8_t padding;
    uint64_t device_id;
} election_header_t;

typedef struct __attribute__((packed)){
    uint32_t service;
    uint32_t group;
    uint16_t priority;
    uint16_t port;
    uint32_t cycles;
    uint8_t flags;
    uint8_t reserved[3];
} election_pkt_t;

#define ELECTION_PKT_FLAGS_LEADER       0x80


typedef struct __attribute__((packed)){
    uint32_t service;
    uint32_t group;
} election_query_t;


void election_v_init( void );
void election_v_handle_shutdown( ip_addr4_t ip );

void election_v_join( uint32_t service, uint32_t group, uint16_t priority, uint16_t port );
void election_v_cancel( uint32_t service );

bool election_b_leader_found( uint32_t service );
sock_addr_t election_a_get_leader( uint32_t service );


#endif


