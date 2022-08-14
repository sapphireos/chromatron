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

NOTES:

IP uses big-endian, AVR uses little-endian

*/


#include "system.h"



#include "config.h"
#include "threading.h"
#include "memory.h"
#include "netmsg.h"
#include "target.h"

#define NO_LOGGING
#include "logging.h"


#include "ip.h"


#ifdef ENABLE_IP

static uint16_t next_ip_id;


// initialize the IP module
void ip_v_init( void ){

	COMPILER_ASSERT( sizeof( ip_hdr_t ) == IP_HEADER_LENGTH_BYTES );
}


// initialize common fields of IP header
void ip_v_init_header( ip_hdr_t *ip_hdr,
                       ip_addr4_t dest_addr,
                       uint8_t protocol,
                       uint8_t ttl,
                       uint16_t len ){

    // set up ip header
	ip_hdr->vhl = ( IP_VERSION << 4 ) | IP_HEADER_LENGTH;
	ip_hdr->ds = 0;
	ip_hdr->flags_offset = 0;

    if( ttl == 0 ){

        ip_hdr->ttl = SOS_IP_DEFAULT_TTL;
	}
    else{

        ip_hdr->ttl = ttl;
    }

    ip_hdr->id = HTONS(next_ip_id);

	next_ip_id++;

    ip_hdr->protocol = protocol;

    ip_hdr->total_length = HTONS( len + sizeof(ip_hdr_t) );

    ip_hdr->dest_addr = dest_addr;

    if( ip_b_check_loopback( ip_hdr->dest_addr ) == TRUE ){

        // if loopback, set source address to loopback
        ip_hdr->source_addr = ip_hdr->dest_addr;
    }
    else{

        // set source address
        cfg_i8_get( CFG_PARAM_IP_ADDRESS, &ip_hdr->source_addr );
    }

    // compute checksum
    ip_hdr->header_checksum = HTONS( ip_u16_ip_hdr_checksum( ip_hdr ) );
}


// check for errors in the IP header
bool ip_b_verify_header( ip_hdr_t *ip_hdr ){

	// check version and header length
	if( ip_hdr->vhl != ( ( IP_VERSION << 4 ) | IP_HEADER_LENGTH ) ){

        log_v_debug_P( PSTR("version") );
		return FALSE;
	}

	// check packet length, minimum is 20 bytes (header with no data),
	// maximum is MAX_IP_PACKET_SIZE
	if( ( HTONS(ip_hdr->total_length) < MIN_IP_PACKET_SIZE ) ||
		( HTONS(ip_hdr->total_length) > MAX_IP_PACKET_SIZE ) ){

        log_v_debug_P( PSTR("len") );
		return FALSE;
	}

	// check fragment offset.  this implementation does not support fragmentation,
	// so make sure the offset is always 0
	if( ( HTONS(ip_hdr->flags_offset) & 0x1FFF ) != 0 ){

        log_v_debug_P( PSTR("frag") );
		return FALSE;
	}

	// check protocol. only ICMP, UDP, and TCP are supported
	if( ( ip_hdr->protocol != IP_PROTO_ICMP ) &&
		( ip_hdr->protocol != IP_PROTO_UDP ) &&
        ( ip_hdr->protocol != IP_PROTO_TCP ) ){

        log_v_debug_P( PSTR("protocol") );
		return FALSE;
	}

	// check the checksum
	if( HTONS(ip_hdr->header_checksum) != ip_u16_ip_hdr_checksum( ip_hdr ) ){

        log_v_debug_P( PSTR("checksum") );
		return FALSE;
	}

	return TRUE;
}

// compute the checksum of the IP header
uint16_t ip_u16_ip_hdr_checksum( ip_hdr_t *ip_hdr ){

	uint16_t ip_checksum = ip_hdr->header_checksum;

	// set ip checksum to 0 in the header
	ip_hdr->header_checksum = 0;

	uint16_t checksum = ip_u16_checksum( ip_hdr, sizeof(ip_hdr_t) );

	// restore the actual checksum in the header
	ip_hdr->header_checksum = ip_checksum;

	// return one's complement of checksum
	return checksum;
}

uint16_t ip_u16_checksum( void *data, uint16_t len ){

	// setup vars
	uint16_t checksum = 0;

	uint8_t *ptr = (uint8_t *)data;

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

	// return one's complement of checksum
	return ~checksum;
}

#endif


ip_addr4_t ip_a_addr( uint8_t ip3, uint8_t ip2, uint8_t ip1, uint8_t ip0 ){

	ip_addr4_t ip;

	ip.ip3 = ip3;
	ip.ip2 = ip2;
	ip.ip1 = ip1;
	ip.ip0 = ip0;

	return ip;
}

bool ip_b_mask_compare( ip_addr4_t subnet_addr,
						ip_addr4_t subnet_mask,
						ip_addr4_t dest_addr ){

	ip_addr4_t masked_subnet;
	ip_addr4_t masked_dest;

    // check if mask is all 0s
    if( ip_b_addr_compare( subnet_mask, ip_a_addr(0,0,0,0) ) ){

        return FALSE;
    }

	// apply the subnet mask to the subnet address
	masked_subnet.ip3 = subnet_addr.ip3 & subnet_mask.ip3;
	masked_subnet.ip2 = subnet_addr.ip2 & subnet_mask.ip2;
	masked_subnet.ip1 = subnet_addr.ip1 & subnet_mask.ip1;
	masked_subnet.ip0 = subnet_addr.ip0 & subnet_mask.ip0;

	// apply the subnet mask to the destination address
	masked_dest.ip3 = dest_addr.ip3 & subnet_mask.ip3;
	masked_dest.ip2 = dest_addr.ip2 & subnet_mask.ip2;
	masked_dest.ip1 = dest_addr.ip1 & subnet_mask.ip1;
	masked_dest.ip0 = dest_addr.ip0 & subnet_mask.ip0;

	// compare the masked subnet to the masked destination
	return ip_b_addr_compare( masked_subnet, masked_dest );
}

bool ip_b_addr_compare( ip_addr4_t addr1, ip_addr4_t addr2 ){

	if( ( addr1.ip3 == addr2.ip3 ) &&
		( addr1.ip2 == addr2.ip2 ) &&
		( addr1.ip1 == addr2.ip1 ) &&
		( addr1.ip0 == addr2.ip0 ) ){

		return TRUE;
	}

	return FALSE;
}

bool ip_b_is_zeroes( ip_addr4_t addr ){

    if( ( addr.ip3 == 0 ) &&
		( addr.ip2 == 0 ) &&
		( addr.ip1 == 0 ) &&
		( addr.ip0 == 0 ) ){

		return TRUE;
	}

	return FALSE;
}

// check if the given ip address is a broadcast for this node's subnet
bool ip_b_check_broadcast( ip_addr4_t dest_addr ){

	ip_addr4_t masked_dest;

	ip_addr4_t subnet;
	cfg_i8_get( CFG_PARAM_IP_SUBNET_MASK, &subnet );

	// check broadcast
	// compliment the subnet
	subnet.ip3 = ~subnet.ip3;
	subnet.ip2 = ~subnet.ip2;
	subnet.ip1 = ~subnet.ip1;
	subnet.ip0 = ~subnet.ip0;

	// apply the subnet mask to the destination address
	masked_dest.ip3 = dest_addr.ip3 & subnet.ip3;
	masked_dest.ip2 = dest_addr.ip2 & subnet.ip2;
	masked_dest.ip1 = dest_addr.ip1 & subnet.ip1;
	masked_dest.ip0 = dest_addr.ip0 & subnet.ip0;

	// if the destination = the compliment of the subnet mask, the packet is
	// a broadcast
	if( ip_b_addr_compare( masked_dest, subnet ) ){

		return TRUE;
	}

	return FALSE;
}


// check if the given ip address is for this node
bool ip_b_check_dest( ip_addr4_t dest_addr ){

	return ip_b_check_loopback( dest_addr );
}

bool ip_b_check_loopback( ip_addr4_t dest_addr ){

	ip_addr4_t my_addr;
	cfg_i8_get( CFG_PARAM_IP_ADDRESS, &my_addr );

	// check unicast
	if( ip_b_addr_compare( dest_addr, my_addr ) ){

		return TRUE;
	}

	// check loopback
	if( ip_b_mask_compare( ip_a_addr(127,0,0,0), ip_a_addr(255,0,0,0), dest_addr ) == TRUE ){

		return TRUE;
	}

	return FALSE;
}

bool ip_b_check_link_local( ip_addr4_t dest_addr ){

	if( ip_b_mask_compare( ip_a_addr(169,254,0,0), ip_a_addr(255,255,0,0), dest_addr ) == TRUE ){

		return TRUE;
	}

	return FALSE;
}

bool ip_b_check_subnet( ip_addr4_t dest_addr ){

    ip_addr4_t my_addr;
    cfg_i8_get( CFG_PARAM_IP_ADDRESS, &my_addr );
    ip_addr4_t subnet;
    cfg_i8_get( CFG_PARAM_IP_SUBNET_MASK, &subnet );

    if( ip_b_mask_compare( my_addr, subnet, dest_addr ) == TRUE ){

        return TRUE;
    }

    return FALSE;
}

uint32_t ip_u32_to_int( ip_addr4_t ip ){

   return ( (uint32_t)ip.ip3 << 24 ) + ( (uint32_t)ip.ip2 << 16 ) + ( ip.ip1 << 8 ) +( ip.ip0 );
}

ip_addr4_t ip_a_from_int( uint32_t i ){

    ip_addr4_t ip;

    ip.ip3 = i >> 24;
    ip.ip2 = i >> 16;
    ip.ip1 = i >> 8;
    ip.ip0 = i;

    return ip;
}
