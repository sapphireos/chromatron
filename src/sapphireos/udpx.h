/*
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
 */

#ifndef __UDPX_H
#define __UDPX_H

#include <stdint.h>

#define UDPX_MAX_TRIES              5 // this is actually 4 tries
#define UDPX_INITIAL_TIMEOUT        2


typedef struct{
    uint8_t flags;
    uint8_t id;
} udpx_header_t;

#define UDPX_FLAGS_VER1         0b10000000
#define UDPX_FLAGS_VER0         0b01000000
#define UDPX_FLAGS_SVR          0b00100000
#define UDPX_FLAGS_ARQ          0b00010000
#define UDPX_FLAGS_ACK          0b00001000
#define UDPX_FLAGS_RSV2         0b00000100
#define UDPX_FLAGS_RSV1         0b00000010
#define UDPX_FLAGS_RSV0         0b00000001

void udpx_v_init_header( udpx_header_t *header, uint8_t flags, uint8_t msg_id );

#endif
