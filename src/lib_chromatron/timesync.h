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

#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

#include "target.h"

#ifdef ENABLE_TIME_SYNC

#include "graphics.h"

#include "wifi_cmd.h"

#define TIME_SERVER_PORT        32037

#define TIME_PROTOCOL_MAGIC     0x454d4954 // 'TIME' in ASCII
#define TIME_PROTOCOL_VERSION   2


#define TIME_SYNC_RATE          4 // in seconds


// Timer
#define TIMESYNC_TIMER                 GFX_TIMER
#define TIMESYNC_TIMER_CC              CCC
#define TIMESYNC_TIMER_CC_VECT         GFX_TIMER_CCC_vect
#define TIMESYNC_TIMER_CC_INTLVL       TC_CCCINTLVL_HI_gc



typedef struct{
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t network_time;
    uint64_t uptime;
} time_msg_sync_t;
#define TIME_MSG_SYNC           1

typedef struct{
    uint32_t magic;
    uint8_t version;
    uint8_t type;
} time_msg_ping_t;
#define TIME_MSG_PING           2

typedef struct{
    uint32_t magic;
    uint8_t version;
    uint8_t type;
} time_msg_ping_reply_t;
#define TIME_MSG_PING_REPLY      3


// typedef struct{
//     uint32_t magic;
//     uint8_t version;
//     uint8_t type;
//     uint32_t sync_group;
//     uint32_t network_time;
//     uint16_t frame_number;
//     uint64_t rng_seed;
//     uint32_t reserved;
//     uint16_t data_index;
//     uint16_t data_count;
//     // register data starts here
// } time_msg_frame_sync_t;
// #define TIME_MSG_FRAME_SYNC     2

void time_v_init( void );
// void time_v_send_frame_sync( wifi_msg_vm_frame_sync_t *sync );
// uint32_t time_u32_get_network_time( void );

#endif
#endif