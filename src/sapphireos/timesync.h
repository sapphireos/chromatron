// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#define TIME_SERVER_PORT                32037

#define TIME_PROTOCOL_MAGIC             0x454d4954 // 'TIME' in ASCII
#define TIME_PROTOCOL_VERSION           7

#define TIME_SOURCE_ESP32_PRIORITY      512

#define TIME_SOURCE_GPS                 64
#define TIME_SOURCE_NTP                 32
#define TIME_SOURCE_RTC                 16
#define TIME_SOURCE_INTERNAL_NTP_SYNC   9 // any source above this level is assumed to have a valid NTP sync
#define TIME_SOURCE_INTERNAL            8
#define TIME_SOURCE_NONE                1

#define TIME_SYNC_RATE_BASE             4 // in seconds
#define TIME_SYNC_RATE_MAX              64 // in seconds

#define TIME_ELECTION_SERVICE           __KV__timesync

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
} time_msg_ping_t;
#define TIME_MSG_PING               1

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
} time_msg_ping_response_t;
#define TIME_MSG_PING_RESPONSE      2

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t id;
} time_msg_request_sync_t;
#define TIME_MSG_REQUEST_SYNC       3

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t source;
    uint32_t net_time;
    uint64_t uptime;
    ntp_ts_t ntp_time;
    uint8_t id;
} time_msg_sync_t;
#define TIME_MSG_SYNC               4


void time_v_init( void );

bool time_b_is_local_sync( void );
bool time_b_is_ntp_sync( void );
uint32_t time_u32_get_network_time( void );
uint32_t time_u32_get_network_time_from_local( uint32_t local_time );
int8_t time_i8_compare_network_time( uint32_t time );
uint32_t time_u32_get_network_aligned( uint32_t alignment );
void time_v_set_gps_sync( bool sync );

ntp_ts_t time_t_from_system_time( uint32_t end_time );
void time_v_set_ntp_master_clock( 
    ntp_ts_t source_ts, 
    uint32_t local_system_time,
    uint8_t source );

void time_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time );
ntp_ts_t time_t_now( void );
ntp_ts_t time_t_local_now( void );

#endif
#endif