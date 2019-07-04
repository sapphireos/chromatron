/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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



#ifndef _IP_H
#define _IP_H

#include "target.h"

#define IP_VERSION 4
#define IP_HEADER_LENGTH 5 // IP options are not supported, so the header length must be 5

#define IP_HEADER_LENGTH_BYTES ( IP_HEADER_LENGTH * 4 )

#define IP_DEFAULT_TTL 64

#define MAX_IP_PACKET_SIZE 576 // this can be changed, but MUST be at least 576!

#define MIN_IP_PACKET_SIZE 20 // NEVER CHANGE THIS! EVER!

// protocols
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17


// convert between network byte order and host
#define HTONS(n) (uint16_t)((((uint16_t) (n)) << 8) | (((uint16_t) (n)) >> 8))
#define HTONL(n) (uint32_t)(((((uint32_t) (n)) << 24) & 0xff000000) | ((((uint32_t) (n)) >> 24) & 0x000000ff) | ((((uint32_t) (n)) << 8) & 0x00ff0000) | ((((uint32_t) (n)) >> 8) & 0x0000ff00))
#define HTONQ(n) (uint64_t) \
(((((uint64_t) (n)) << 56) & 0xff00000000000000) | ((((uint64_t) (n)) >> 56) & 0x00000000000000ff) | \
 ((((uint64_t) (n)) << 40) & 0x00ff000000000000) | ((((uint64_t) (n)) >> 40) & 0x000000000000ff00) | \
 ((((uint64_t) (n)) << 24) & 0x0000ff0000000000) | ((((uint64_t) (n)) >> 24) & 0x0000000000ff0000) | \
 ((((uint64_t) (n)) <<  8) & 0x000000ff00000000) | ((((uint64_t) (n)) >>  8) & 0x00000000ff000000))


typedef struct{
	uint8_t ip3;
	uint8_t ip2;
	uint8_t ip1;
	uint8_t ip0;
} ip_addr_t;

typedef struct{
	uint8_t vhl; // version and header length
	uint8_t ds; // differentiated services (unused)
	uint16_t total_length; // total length of IP packet
	uint16_t id; // packet ID (unused)
	uint16_t flags_offset; // flags (3 bits) and offset (13 bits)
	uint8_t ttl; // time to live
	uint8_t protocol; // next protocol
	uint16_t header_checksum; // self-explanatory
	ip_addr_t source_addr;
	ip_addr_t dest_addr;
} ip_hdr_t;


#ifdef ENABLE_IP
void ip_v_init( void );

void ip_v_init_header( ip_hdr_t *ip_hdr,
                       ip_addr_t dest_addr,
                       uint8_t protocol,
                       uint8_t ttl,
                       uint16_t len );

bool ip_b_verify_header( ip_hdr_t *ip_hdr );
uint16_t ip_u16_ip_hdr_checksum( ip_hdr_t *ip_hdr );
uint16_t ip_u16_checksum( void *data, uint16_t len );
#endif

ip_addr_t ip_a_addr( uint8_t ip3, uint8_t ip2, uint8_t ip1, uint8_t ip0 );
bool ip_b_is_zeroes( ip_addr_t addr );
bool ip_b_mask_compare( ip_addr_t subnet_addr,
						ip_addr_t subnet_mask,
						ip_addr_t dest_addr );
bool ip_b_addr_compare( ip_addr_t addr1, ip_addr_t addr2 );
bool ip_b_check_broadcast( ip_addr_t dest_addr );
bool ip_b_check_dest( ip_addr_t dest_addr );
bool ip_b_check_loopback( ip_addr_t dest_addr );
bool ip_b_check_link_local( ip_addr_t dest_addr );
uint32_t ip_u32_to_int( ip_addr_t ip );
ip_addr_t ip_a_from_int( uint32_t i );

#endif
