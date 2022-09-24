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

#ifndef __SERVICES_H
#define __SERVICES_H

#include "sockets.h"

#define SERVICES_PORT                       32041
#define SERVICES_MAGIC                      0x56524553 // 'SERV'
#define SERVICES_VERSION                    3

#define SERVICES_MCAST_ADDR                 239,43,96,31

#define SERVICE_RATE                        4000
#define SERVICE_UPTIME_MIN_DIFF             5

#define SERVICE_LISTEN_TIMEOUT              10
#define SERVICE_CONNECTED_TIMEOUT           32
#define SERVICE_CONNECTED_PING_THRESHOLD    16
#define SERVICE_CONNECTED_WARN_THRESHOLD    4

#define SERVICE_PRIORITY_FOLLOWER_ONLY      0

#define SERVICE_MAX_FILE_SIZE               512

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t reserved;
    uint64_t origin;
} service_msg_header_t;
#define SERVICE_FLAGS_SHUTDOWN          0x80


#define SERVICE_MSG_TYPE_OFFERS     1
typedef struct __attribute__((packed)){
    uint8_t count;
    uint8_t reserved[3];
} service_msg_offer_hdr_t;

typedef struct __attribute__((packed)){
    uint32_t id;
    uint64_t group;
    uint16_t priority;
    uint16_t port;
    uint32_t uptime;
    uint8_t flags;
    uint8_t reserved[3];
} service_msg_offer_t;
#define SERVICE_OFFER_FLAGS_TEAM        0x01
#define SERVICE_OFFER_FLAGS_SERVER      0x02

#define SERVICE_MSG_TYPE_QUERY      2
typedef struct __attribute__((packed)){
    uint32_t id;
    uint64_t group;
} service_msg_query_t;


void services_v_init( void );

void services_v_listen( uint32_t id, uint64_t group );
void services_v_offer( uint32_t id, uint64_t group, uint16_t priority, uint16_t port );
void services_v_join_team( uint32_t id, uint64_t group, uint16_t priority, uint16_t port );
void services_v_cancel( uint32_t id, uint64_t group );

bool services_b_is_available( uint32_t id, uint64_t group );
bool services_b_is_server( uint32_t id, uint64_t group );

sock_addr_t services_a_get( uint32_t id, uint64_t group );
ip_addr4_t services_a_get_ip( uint32_t id, uint64_t group );
uint16_t services_u16_get_port( uint32_t id, uint64_t group );
uint16_t services_u16_get_leader_priority( uint32_t id, uint64_t group );


#endif


