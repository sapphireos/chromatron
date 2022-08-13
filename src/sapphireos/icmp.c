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

Internet Control Messaging Protocol

This is a minimal ping and tracert only implementation

*/


#include "system.h"

#ifdef ENABLE_IP

#include "netmsg.h"

#include "icmp.h"

#define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"



netmsg_t icmp_nm_create( icmp_hdr_t *icmp_hdr,
                         ip_addr_t dest_addr,
                         uint8_t *data,
                         uint16_t size ){

    // create netmsg object
    netmsg_t netmsg = netmsg_nm_create( NETMSG_TYPE_ICMP );

    // check allocation
    if( netmsg < 0 ){

        return -1;
    }

    // create data handle
    mem_handle_t data_h = mem2_h_alloc( sizeof(icmp_hdr_t) + size );

    if( data_h < 0 ){

        netmsg_v_release( netmsg );

        return -2;
    }

    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    state->raddr.ipaddr = dest_addr;
    state->data_handle = data_h;

    void *ptr = mem2_vp_get_ptr( data_h );

    // copy ICMP header
    memcpy( ptr, icmp_hdr, sizeof(icmp_hdr_t) );

    ptr += sizeof(icmp_hdr_t);

    // copy data
    memcpy( ptr, data, size );

    return netmsg;
}



void icmp_v_recv( netmsg_t netmsg ){

    // check security flags on netmsg
    // we only accept ICMP messages which are secured
    if( netmsg_u8_get_flags( netmsg ) & NETMSG_FLAGS_WCOM_SECURITY_DISABLE ){

        return;
    }

    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    if( state->data_handle < 0 ){

        return;
    }


	// get icmp header
	icmp_hdr_t *icmp_hdr = mem2_vp_get_ptr( state->data_handle ) + sizeof(ip_hdr_t);

	// compute icmp size
	uint16_t icmp_size = mem2_u16_get_size( state->data_handle ) - sizeof(ip_hdr_t);

	if( HTONS( icmp_hdr->checksum ) == icmp_u16_checksum( icmp_hdr, icmp_size ) ){

		if( icmp_hdr->type == ICMP_TYPE_ECHO_REQUEST ){

			// modify the existing packet to form the reply
			icmp_hdr->type = ICMP_TYPE_ECHO_REPLY;

			// recompute checksum
			icmp_hdr->checksum = HTONS( icmp_u16_checksum( icmp_hdr, icmp_size ) );

			uint8_t *data = ( uint8_t * )( (void *)icmp_hdr + sizeof(icmp_hdr_t) );

            // create netmsg
			netmsg_t netmsg = icmp_nm_create( icmp_hdr,
                                              state->raddr.ipaddr,
                                              data,
                                              icmp_size - sizeof(icmp_hdr_t) );

            // check creation, and send if created
            if( netmsg >= 0 ){

                netmsg_v_send( netmsg );
            }
		}
	}
}


void icmp_v_send_ttl_exceeded( ip_hdr_t *ip_hdr ){

	// create ICMP message
	icmp_t icmp;

	// create the ttl message
	icmp.ip_hdr = *ip_hdr;

	// get first 64 bits of data
	uint8_t *ptr = (void *)ip_hdr + sizeof(ip_hdr_t);

	memcpy( icmp.data, ptr, sizeof( icmp.data ) );

	// build ICMP header
	icmp.hdr.type = ICMP_TYPE_TIME_EXCEEDED;
	icmp.hdr.code = ICMP_CODE_TTL_EXCEEDED;
	icmp.hdr.id = 0;
	icmp.hdr.sequence = 0;

	// compute checksum
	icmp.hdr.checksum = HTONS( icmp_u16_checksum( &icmp.hdr, sizeof( icmp ) ) );

	netmsg_t netmsg = icmp_nm_create( &icmp.hdr,
                                      icmp.ip_hdr.source_addr,
                                      (uint8_t *)&icmp.ip_hdr,
                                      sizeof(icmp.data) + sizeof(icmp.ip_hdr) );

    // check netmsg creation and set to send if successful
    if( netmsg >= 0 ){

        netmsg_v_send( netmsg );
    }
}


void icmp_v_send_dest_unreachable( ip_hdr_t *ip_hdr ){

    // create ICMP message
	icmp_t icmp;

	// create the ttl message
	icmp.ip_hdr = *ip_hdr;

	// get first 64 bits of data
	uint8_t *ptr = (void *)ip_hdr + sizeof(ip_hdr_t);

	memcpy( icmp.data, ptr, sizeof( icmp.data ) );

	// build ICMP header
	icmp.hdr.type = ICMP_TYPE_DEST_UNREACHABLE;
	icmp.hdr.code = ICMP_CODE_DEST_UNREACHABLE;
	icmp.hdr.id = 0;
	icmp.hdr.sequence = 0;

	// compute checksum
	icmp.hdr.checksum = HTONS( icmp_u16_checksum( &icmp.hdr, sizeof( icmp ) ) );

	netmsg_t netmsg = icmp_nm_create( &icmp.hdr,
                                      icmp.ip_hdr.source_addr,
                                      (uint8_t *)&icmp.ip_hdr,
                                      sizeof(icmp.data) + sizeof(icmp.ip_hdr) );

    // check netmsg creation and set to send if successful
    if( netmsg >= 0 ){

        netmsg_v_send( netmsg );
    }
}


uint16_t icmp_u16_checksum( icmp_hdr_t *icmp_hdr, uint16_t total_length ){

	uint16_t icmp_checksum = icmp_hdr->checksum;

	// set checksum in header to 0
	icmp_hdr->checksum = 0;

	// compute checksum
	uint16_t checksum = ip_u16_checksum( icmp_hdr, total_length );

	// restore checksum in header
	icmp_hdr->checksum = icmp_checksum;

	return checksum;
}

#endif
