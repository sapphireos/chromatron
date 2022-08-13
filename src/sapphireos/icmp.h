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


#ifndef _ICMP_H
#define _ICMP_H

#include "target.h"

#ifdef ENABLE_IP

#include "ip.h"
#include "netmsg.h"

typedef struct{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t id;
	uint16_t sequence;
} icmp_hdr_t;

typedef struct{
	icmp_hdr_t hdr;
	ip_hdr_t ip_hdr;
	uint8_t data[8];
} icmp_t;


#define ICMP_TYPE_ECHO_REPLY 			0
#define ICMP_TYPE_DEST_UNREACHABLE		3
#define ICMP_TYPE_ECHO_REQUEST 			8
#define ICMP_TYPE_TIME_EXCEEDED			11

#define ICMP_CODE_TTL_EXCEEDED          0
#define ICMP_CODE_NETWORK_UNREACHABLE   0
#define ICMP_CODE_DEST_UNREACHABLE      1
#define ICMP_CODE_PROTOCOL_UNREACHABLE  2
#define ICMP_CODE_PORT_UNREACHABLE      3



netmsg_t icmp_nm_create( icmp_hdr_t *icmp_hdr,
                         ip_addr_t dest_addr, 
                         uint8_t *data, 
                         uint16_t size );

void icmp_v_recv( netmsg_t netmsg );
void icmp_v_send_ttl_exceeded( ip_hdr_t *ip_hdr );
void icmp_v_send_dest_unreachable( ip_hdr_t *ip_hdr );

uint16_t icmp_u16_checksum( icmp_hdr_t *icmp_hdr, uint16_t total_length );


#endif

#endif
