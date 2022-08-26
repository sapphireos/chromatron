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


#ifndef _SNTP_H
#define _SNTP_H

#include "ntp.h"

#define SNTP_SERVER_PORT    123

// minimum poll interval in seconds
// changing (reducing) this violates the RFC
#define SNTP_MINIMUM_POLL_INTERVAL      15

#define SNTP_DEFAULT_POLL_INTERVAL      600

#define SNTP_TIMEOUT                    4


// NTP Packet
typedef struct __attribute__((packed)){
    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    int8_t precision;
    int32_t root_delay;
    uint32_t root_dispersion;
    uint32_t reference_identifier;
    ntp_ts_t reference_timestamp;
    ntp_ts_t originate_timestamp;
    ntp_ts_t receive_timestamp;
    ntp_ts_t transmit_timestamp;
    // the key identifer and digest are not implemented
} ntp_packet_t;

#define SNTP_LI_MASK            0b11000000
#define SNTP_LI_NO_WARNING      0b00000000
#define SNTP_LI_61_SECONDS      0b01000000
#define SNTP_LI_59_SECONDS      0b10000000
#define SNTP_LI_ALARM           0b11000000

#define SNTP_VN_MASK            0b00111000
#define SNTP_VERSION_4          0b00100000

#define SNTP_MODE_MASK          0b00000111
#define SNTP_MODE_CLIENT        0b00000011
#define SNTP_MODE_SERVER        0b00000100
#define SNTP_MODE_BROADCAST     0b00000101

#define SNTP_STRATUM_KOD        0 // kiss of death
#define SNTP_STRATUM_PRIMARY    1
#define SNTP_STRATUM_SECONDARY  2 // to 15
#define SNTP_STRATUM_RESERVED   16 // to 255

typedef int8_t sntp_status_t8;
#define SNTP_STATUS_DISABLED            -1
#define SNTP_STATUS_NO_SYNC             0
#define SNTP_STATUS_SYNCHRONIZED        1


void sntp_v_init( void );
void sntp_v_start( void );
void sntp_v_stop( void );

sntp_status_t8 sntp_u8_get_status( void );

int16_t sntp_i16_get_offset( void );
uint16_t sntp_u16_get_delay( void );

#endif
