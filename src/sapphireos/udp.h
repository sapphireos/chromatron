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



#ifndef _UDP_H
#define _UDP_H

#include "target.h"
#include "netmsg.h"
#include "ip.h"

typedef struct __attribute__((packed)){
	uint16_t source_port;
	uint16_t dest_port;
	uint16_t length;
	uint16_t checksum;
} udp_header_t;

#define UDP_MAX_LEN ( MAX_IP_PACKET_SIZE - MIN_IP_PACKET_SIZE - sizeof(udp_header_t) )

#ifdef ENABLE_IP

#include "ip.h"

// header used for the UDP checksum algorithm for IPv4
typedef struct{
	ip_addr_t source_addr;
	ip_addr_t dest_addr;
	uint8_t zeroes;
	uint8_t protocol;
	uint16_t udp_length;
	//udp_header_t udp_header;
} udp_checksum_header_t;


void udp_v_init_header( udp_header_t *header,
                        uint16_t source_port,
                        uint16_t dest_port,
                        uint16_t size );

uint16_t udp_u16_checksum( ip_hdr_t *ip_hdr );
uint16_t udp_u16_checksum_netmsg( netmsg_t netmsg );


#endif

#endif
