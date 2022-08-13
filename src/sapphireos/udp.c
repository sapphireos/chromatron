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

/*

User Datagram Protocol Packet Processing Routines

*/


#include "system.h"

#include "netmsg.h"

#include "ip.h"
#include "udp.h"

void udp_v_init_header( udp_header_t *header,
                        uint16_t source_port,
                        uint16_t dest_port,
                        uint16_t size ){

	// setup the header
	header->source_port = HTONS(source_port);
	header->dest_port = HTONS(dest_port);
	header->length = HTONS(size + sizeof(udp_header_t));
	header->checksum = 0;
}


#ifdef ENABLE_IP

uint16_t udp_u16_checksum( ip_hdr_t *ip_hdr ){

	// get the udp header
	udp_header_t *udp_hdr = (udp_header_t *)( (void*)ip_hdr + sizeof(ip_hdr_t) );

    uint16_t original_checksum = udp_hdr->checksum;
    udp_hdr->checksum = 0;

	// create the udp/ipv4 pseudoheader
	udp_checksum_header_t hdr;

	hdr.source_addr = ip_hdr->source_addr;
	hdr.dest_addr = ip_hdr->dest_addr;
	hdr.zeroes = 0;
	hdr.protocol = IP_PROTO_UDP;
	hdr.udp_length = udp_hdr->length;

	// run the checksum over the pseudoheader
	uint16_t checksum = 0;
	uint8_t *ptr = (uint8_t *)&hdr;
	uint16_t len = sizeof(udp_checksum_header_t);

	while( len > 1 ){

		uint16_t temp = ( ptr[0] << 8 ) + ptr[1];

		checksum += temp;

		if( checksum < temp ){

			checksum++; // add carry
		}

		ptr += 2;
		len -= 2;
	}

	// run the checksum over the rest of the datagram
	ptr = (uint8_t *)udp_hdr;
	len = HTONS(udp_hdr->length);

	while( len > 1 ){

		uint16_t temp = ( ptr[0] << 8 ) + ptr[1];

		checksum += temp;

		if( checksum < temp ){

			checksum++; // add carry
		}

		ptr += 2;
		len -= 2;
	}

	// check if last byte
	if( len == 1 ){

		uint16_t temp = ( ptr[0] << 8 );

		checksum += temp;

		if( checksum < temp ){

			checksum++; // add carry
		}
	}

    udp_hdr->checksum = original_checksum;

	// return one's complement of checksum
	return ~checksum;
}

uint16_t udp_u16_checksum_netmsg( netmsg_t netmsg ){

    
}


#endif
