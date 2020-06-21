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

#define ELECTION_PORT 		32036
#define ELECTION_MAGIC		0x45544f56 // 'VOTE'
#define ELECTION_VERSION	1

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t count;
    uint8_t padding;
    uint64_t device_id;
    uint64_t uptime;
} election_header_t;

typedef struct __attribute__((packed)){
	uint32_t service;
	uint16_t priority;
	uint16_t port;
} election_pkt_t;


void election_v_init( void );

void election_v_create( uint32_t service, uint16_t priority, uint16_t port );
void election_v_cancel( uint32_t service );

#endif

