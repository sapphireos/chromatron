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

#ifndef __CONTROLLER_H
#define __CONTROLLER_H

void controller_v_init( void );

#define CONTROLLER_PORT         44701

#define CONTROLLER_IDLE_TIMEOUT         10
#define CONTROLLER_FOLLOWER_TIMEOUT     20
#define CONTROLLER_ELECTION_TIMEOUT     5


#define CONTROLLER_MSG_MAGIC    0x4C525443 // 'CTRL'
#define CONTROLLER_MSG_VERSION  1
typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t msg_type;
    uint8_t version;
    uint16_t reserved;
} controller_header_t;


#define CONTROLLER_FLAGS_IS_LEADER      0x0001
// #define CONTROLLER_FLAGS_DROP_LEADER    0x0010

typedef struct __attribute__((packed)){
    controller_header_t header;
    uint16_t flags;
    uint16_t priority;
    uint16_t follower_count;
    // uint64_t uptime;
    // uint64_t device_id;
}  controller_msg_announce_t;
#define CONTROLLER_MSG_ANNOUNCE     1


typedef struct __attribute__((packed)){
    controller_header_t header;
}  controller_msg_drop_t;
#define CONTROLLER_MSG_DROP         2

typedef struct __attribute__((packed)){
    controller_header_t header;
    
    
}  controller_msg_status_t;
#define CONTROLLER_MSG_STATUS       3

typedef struct __attribute__((packed)){
    controller_header_t header;
}  controller_msg_leave_t;
#define CONTROLLER_MSG_LEAVE        4

#endif




