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

#ifndef _TIME_NTP_H_
#define _TIME_NTP_H_

#include "target.h"

#ifdef ENABLE_TIME_SYNC

#define NTP_PROTOCOL_MAGIC             0x50544e53 // 'SNTP' in ASCII
#define NTP_PROTOCOL_VERSION           1

#define NTP_SERVER_PORT                32038

#define NTP_ELECTION_SERVICE           __KV__ntpclock

#define NTP_SYNC_INTERVAL              600 // in seconds

// #define NTP_MASTER_CLOCK_TIMEOUT       60 // for debug
#define NTP_MASTER_CLOCK_TIMEOUT       3600 // in seconds

#define NTP_HARD_SYNC_THRESHOLD_MS     2000 // in ms

#define NTP_INITIAL_SNTP_SYNC_TIMEOUT  120 // in seconds

// directly attached GPS source:
#define NTP_SOURCE_GPS                 32
// network sync to GPS source:
#define NTP_SOURCE_GPS_NET             30
// direct SNTP sync:
#define NTP_SOURCE_SNTP                16
// network sync to SNTP source:
#define NTP_SOURCE_SNTP_NET            14

// internal clock sync.
// this is a system that previously had 
// a reference clock sync (GPS/SNTP/manual setting)
// and has since lost a direct connection to the
// reference clock, but still retains the prior
// clock setting.
#define NTP_SOURCE_INTERNAL            8
#define NTP_SOURCE_INTERNAL_NET        6

// manually set clock
#define NTP_SOURCE_MANUAL              4

#define NTP_SOURCE_NONE                1
#define NTP_SOURCE_INVALID             0


typedef struct  __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint64_t origin_system_time_ms;
} ntp_msg_request_sync_t;
#define NTP_MSG_REQUEST_SYNC           1

typedef struct  __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t source;
    uint64_t origin_system_time_ms;
    ntp_ts_t ntp_timestamp;
} ntp_msg_reply_sync_t;
#define NTP_MSG_REPLY_SYNC             2



void ntp_v_init( void );

void ntp_v_set_master_clock( 
    ntp_ts_t source_ntp, 
    uint64_t local_system_time_ms,
    uint8_t source );

void ntp_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time );
ntp_ts_t ntp_t_from_system_time( uint64_t sys_time_ms );
ntp_ts_t ntp_t_now( void );
ntp_ts_t ntp_t_local_now( void );
bool ntp_b_is_sync( void );

#endif

#endif