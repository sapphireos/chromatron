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

#include "ntp.h"
#include "graphics.h"

#include "wifi_cmd.h"

#define TIME_SERVER_PORT                32037

#define TIME_PROTOCOL_MAGIC             0x454d4954 // 'TIME' in ASCII
#define TIME_PROTOCOL_VERSION           2

#define TIME_FLAGS_SYNC                 0x01
#define TIME_FLAGS_SOURCE_GPS           0x80
#define TIME_FLAGS_SOURCE_NTP           0x40


#define TIME_MASTER_SYNC_RATE           4 // in seconds
#define TIME_SLAVE_SYNC_RATE_BASE       4 // in seconds
#define TIME_SLAVE_SYNC_RATE_MAX        32 // in seconds

// Timer
#define TIMESYNC_TIMER                 GFX_TIMER
#define TIMESYNC_TIMER_CC              CCC
#define TIMESYNC_TIMER_CC_VECT         GFX_TIMER_CCC_vect
#define TIMESYNC_TIMER_CC_INTLVL       TC_CCCINTLVL_HI_gc

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint32_t net_time;
    uint64_t uptime;
} time_msg_master_t;
#define TIME_MSG_MASTER             1

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
} time_msg_not_master_t;
#define TIME_MSG_NOT_MASTER         2

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
} time_msg_request_sync_t;
#define TIME_MSG_REQUEST_SYNC       3

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint32_t net_time;
    uint64_t uptime;
    ntp_ts_t ntp_time;
} time_msg_sync_t;
#define TIME_MSG_SYNC               4


void time_v_init( void );
uint32_t time_u32_get_network_time( void );

#endif
#endif