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


#include "system.h"
#include "list.h"
#include "icmp.h"
#include "udp.h"
#include "sockets.h"
#include "keyvalue.h"

#include "netmsg.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

static uint32_t netmsg_udp_sent;
static uint32_t netmsg_udp_recv;

#if defined(__SIM__) || defined(BOOTLOADER)
    #define ROUTING_TABLE_START
#else
    #define ROUTING_TABLE_START       __attribute__ ((section (".routing_start"), used))
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define ROUTING_TABLE_END
#else
    #define ROUTING_TABLE_END         __attribute__ ((section (".routing_end"), used))
#endif


KV_SECTION_META kv_meta_t netmsg_info_kv[] = {
    { SAPPHIRE_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,  &netmsg_udp_sent,    0,   "netmsg_udp_sent" },
    { SAPPHIRE_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,  &netmsg_udp_recv,    0,   "netmsg_udp_recv" },
};


void ( *netmsg_v_receive_msg )( netmsg_t msg );

void default_open_close_port( uint8_t protocol, uint16_t port, bool open ){
}

int8_t loopback_i8_get_route( ip_addr_t *subnet, ip_addr_t *subnet_mask ){

    *subnet = ip_a_addr(127,0,0,0);
    *subnet_mask = ip_a_addr(255,0,0,0);

    return 0;
}

int8_t loopback_i8_transmit( netmsg_t msg ){

    netmsg_state_t *state = netmsg_vp_get_state( msg );

    // switch addresses
    sock_addr_t temp_addr = state->raddr;
    state->raddr = state->laddr;
    state->laddr = temp_addr;    

    netmsg_v_receive_msg( msg );

    return NETMSG_TX_OK_NORELEASE;
}

// first entry is loopback interface
ROUTING_TABLE_START routing_table_entry_t route_start = {
    loopback_i8_get_route,
    loopback_i8_transmit,
    default_open_close_port,
    0
};

ROUTING_TABLE_END routing_table_entry_t route_end[] = {};

// initialize netmsg
void netmsg_v_init( void ){

    netmsg_v_receive_msg = netmsg_v_receive;
}

uint8_t _netmsg_u8_header_size( netmsg_type_t type ){

    return sizeof(netmsg_state_t);
}

netmsg_t netmsg_nm_create( netmsg_type_t type ){

    uint8_t header_size = _netmsg_u8_header_size( type );

    // create list node
    list_node_t n = list_ln_create_node2( 0, header_size, MEM_TYPE_NETMSG );

    // check handle
    if( n < 0 ){

		return -1;
    }

    // get state pointer and initialize
	netmsg_state_t *msg = list_vp_get_data( n );

    // init flags
    msg->flags = 0;
    msg->type = type;
    msg->header_0_handle = -1;
    msg->header_1_handle = -1;
    msg->header_2_handle = -1;
    msg->header_len = 0;
    msg->data_handle = -1;
    msg->ttl = 0;
    memset( &msg->laddr, 0, sizeof(msg->laddr) );
    memset( &msg->raddr, 0, sizeof(msg->raddr) );

	return n;
}

void netmsg_v_release( netmsg_t netmsg ){

    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    if( state->header_0_handle >= 0 ){

        mem2_v_free( state->header_0_handle );
    }

    if( state->header_1_handle >= 0 ){

        mem2_v_free( state->header_1_handle );
    }

    if( state->header_2_handle >= 0 ){

        mem2_v_free( state->header_2_handle );
    }

    if( state->data_handle >= 0 ){

        if( ( state->flags & NETMSG_FLAGS_RETAIN_DATA ) == 0 ){

            mem2_v_free( state->data_handle );
        }
    }

    list_v_release_node( netmsg );
}

void netmsg_v_set_flags( netmsg_t netmsg, netmsg_flags_t flags ){

    // get state pointer
    netmsg_state_t *msg = netmsg_vp_get_state( netmsg );

    msg->flags = flags;
}

netmsg_flags_t netmsg_u8_get_flags( netmsg_t netmsg ){

    // get state pointer
    netmsg_state_t *msg = netmsg_vp_get_state( netmsg );

    return msg->flags;
}


void netmsg_v_receive( netmsg_t netmsg ){

    // get state pointer
    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    if( state->type == NETMSG_TYPE_UDP ){

        #ifdef ENABLE_IP

        // get data
        uint8_t *data = mem2_vp_get_ptr( state->data_handle );

        // get headers
        ip_hdr_t *ip_hdr = (ip_hdr_t *)data;
        data += sizeof(ip_hdr_t);
        state->header_len += sizeof(ip_hdr_t);

        // check the IP header
        if( ip_b_verify_header( ip_hdr ) != TRUE ){

            log_v_debug_P( PSTR("IPv4 header fail: %d.%d.%d.%d -> %d.%d.%d.%d"),
                ip_hdr->source_addr.ip3,
                ip_hdr->source_addr.ip2,
                ip_hdr->source_addr.ip1,
                ip_hdr->source_addr.ip0,
                ip_hdr->dest_addr.ip3,
                ip_hdr->dest_addr.ip2,
                ip_hdr->dest_addr.ip1,
                ip_hdr->dest_addr.ip0 );

            goto clean_up;
        }

        // check data length matches ipv4 header
        if( mem2_u16_get_size( state->data_handle ) != HTONS( ip_hdr->total_length ) ){

            log_v_debug_P( PSTR("IPv4 len mismatch: %u != %u"), mem2_u16_get_size( state->data_handle ), HTONS( ip_hdr->total_length ) );

            goto clean_up;
        }

        bool ip_broadcast = ip_b_check_broadcast( ip_hdr->dest_addr );
        bool ip_receive = ip_b_check_dest( ip_hdr->dest_addr );

        // log_v_debug_P( PSTR("IPv4 header: %d.%d.%d.%d -> %d.%d.%d.%d %d"),
        //     ip_hdr->source_addr.ip3,
        //     ip_hdr->source_addr.ip2,
        //     ip_hdr->source_addr.ip1,
        //     ip_hdr->source_addr.ip0,
        //     ip_hdr->dest_addr.ip3,
        //     ip_hdr->dest_addr.ip2,
        //     ip_hdr->dest_addr.ip1,
        //     ip_hdr->dest_addr.ip0,
        //     ip_receive );
        //


        // check destination address
        // check if packet is for this node
        if( ( ip_receive == TRUE ) || ( ip_broadcast == TRUE ) ){

            // set addresses
            state->raddr.ipaddr = ip_hdr->source_addr;
            state->laddr.ipaddr = ip_hdr->dest_addr;


            // does this make sense?  should we have an IP packet instead
            // of UDP for this?

            // set the netmsg protocol for the message router
            if( ip_hdr->protocol == IP_PROTO_ICMP ){

                // call the icmp receive handler
                icmp_v_recv( netmsg );
            }
            else if( ip_hdr->protocol == IP_PROTO_UDP ){

                udp_header_t *udp_hdr = (udp_header_t *)data;
                data += sizeof(udp_header_t);
                state->header_len += sizeof(udp_header_t);

                // log_v_debug_P( PSTR("header_len %d size: %d"), state->header_len, mem2_u16_get_size( state->data_handle ) );

                // check UDP checksum, if checksum is enabled
                // if( ( udp_hdr->checksum != 0 ) &&
                //     ( udp_u16_checksum( ip_hdr ) != HTONS(udp_hdr->checksum) ) ){
                //
                //     goto clean_up;
                // }

                // set ports
                state->raddr.port = HTONS(udp_hdr->source_port);
                state->laddr.port = HTONS(udp_hdr->dest_port);

                sock_v_recv( netmsg );
            }
            else{

                goto clean_up;
            }
        }
        else{

            goto clean_up;
        }
        #else

        // easy mode!

        sock_v_recv( netmsg );

        #endif

        netmsg_udp_recv++;
    }


#ifdef ENABLE_IP
clean_up:
#endif
    netmsg_v_release( netmsg );
}

void netmsg_v_open_close_port( uint8_t protocol, uint16_t port, bool open ){

    routing_table_entry_t *ptr = &route_start;
    routing_table_entry_t route;    

    // iterate through routes to find a match
    while( ptr < route_end ){

        // load meta data
        memcpy_P( &route, ptr, sizeof(route) );

        if( route.open_close_port != 0 ){
            
            route.open_close_port( protocol, port, open );
        }
        
        ptr++;
    }
}

int8_t netmsg_i8_transmit_msg( netmsg_t msg ){

    netmsg_state_t *state = netmsg_vp_get_state( msg );    

    routing_table_entry_t *ptr = &route_start;
    routing_table_entry_t route;    

    routing_table_entry_t default_route;
    default_route.tx_func = 0;

    // iterate through routes to find a match
    while( ptr < route_end ){

        // load meta data
        memcpy_P( &route, ptr, sizeof(route) );
    
        ip_addr_t subnet;
        ip_addr_t subnet_mask;

        int8_t route_status = route.get_route( &subnet, &subnet_mask );
        if( route_status < 0 ){

            goto next;
        }

        // check if default route
        if( route_status == NETMSG_ROUTE_DEFAULT ){
   
            memcpy_P( &default_route, ptr, sizeof(route) );      
        }

        // check if global broadcast
        bool global_broadcast = ip_b_addr_compare( state->raddr.ipaddr, ip_a_addr(255,255,255,255) );

        // check subnet
        if( ip_b_mask_compare( subnet, subnet_mask, state->raddr.ipaddr ) ||
            global_broadcast ){

            // check if global broadcast and if permissions flag is set
            if( global_broadcast &&
                ( ( route.flags & ROUTE_FLAGS_ALLOW_GLOBAL_BROADCAST ) == 0 ) ){

                goto next;
            }

            return route.tx_func( msg );
        }
        
            
next:
        ptr++;
    }

    // check if default route available:
    if( default_route.tx_func != 0 ){

        return default_route.tx_func( msg );    
    }

    // log_v_debug_P( PSTR("no route found") );

    // no route found
    return NETMSG_TX_ERR_RELEASE;
}

int8_t netmsg_i8_send( netmsg_t netmsg ){

    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    if( state->type == NETMSG_TYPE_ICMP ){

        #ifdef ENABLE_IP

        // allocate header 1
        mem_handle_t ip_hdr_handle = mem2_h_alloc( sizeof(ip_hdr_t) );

        if( ip_hdr_handle < 0 ){

            goto clean_up;
        }

        state->header_1_handle = ip_hdr_handle;

        uint16_t data_size = mem2_u16_get_size( state->data_handle );

        ip_hdr_t *ip_hdr = mem2_vp_get_ptr( ip_hdr_handle );

        ip_v_init_header( ip_hdr,
                          state->raddr.ipaddr,
                          IP_PROTO_ICMP,
                          0,
                          data_size );

        #endif
    }
    else if( state->type == NETMSG_TYPE_UDP ){

        // for now, extra header processing selected based on
        // compiler tokens.  eventually should do this based on interface.

        #ifdef ENABLE_IP

        mem_handle_t ip_udp_hdr_handle = mem2_h_alloc( sizeof(ip_hdr_t) + sizeof(udp_header_t) );

        if( ip_udp_hdr_handle < 0 ){

            goto clean_up;
        }

        state->header_1_handle = ip_udp_hdr_handle;

        ip_hdr_t *ip_hdr = mem2_vp_get_ptr( ip_udp_hdr_handle );

        uint16_t data_size = mem2_u16_get_size( state->data_handle );

        // check if header2 has any data
        if( state->header_2_handle > 0 ){

            data_size += mem2_u16_get_size( state->header_2_handle );
        }

        ip_v_init_header( ip_hdr,
                          state->raddr.ipaddr,
                          IP_PROTO_UDP,
                          state->ttl,
                          data_size + sizeof(udp_header_t) );

        udp_header_t *udp_hdr = (udp_header_t *)( ip_hdr + 1 );

        udp_v_init_header( udp_hdr,
                           state->laddr.port,
                           state->raddr.port,
                           data_size );



        // TODO!!!! Need to create a checksum routine that works on memory buffers
        // ASSERT( 0 );

        // compute checksum
        // udp_hdr->checksum = HTONS(udp_u16_checksum_netmsg( netmsg ));
        udp_hdr->checksum = 0;

        #else

        // non-IP, easy mode!

        #endif

        netmsg_udp_sent++;
    }
    else{

        ASSERT( 0 );
    }

    // attempt to transmit
    int8_t status = netmsg_i8_transmit_msg( netmsg );

    if( status < 0 ){

        // codes less than 0 mean do not release netmsg.

        // return 0 if OK, -1 if error
        if( status == NETMSG_TX_ERR_NORELEASE ){

            return -1;
        }

        return 0;
    }

#ifdef ENABLE_IP
clean_up:
#endif

    // release message
    netmsg_v_release( netmsg );

    // return 0 if OK, -1 if error
    if( status == NETMSG_TX_ERR_RELEASE ){

        return -1;
    }

    return 0;
}



#ifdef ENABLE_EXTENDED_VERIFY
void *_netmsg_vp_get_state( netmsg_t netmsg, FLASH_STRING_T file, int line ){

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( netmsg ) == FALSE ){

        assert( 0, file, line );
    }

#else
void *netmsg_vp_get_state( netmsg_t netmsg ){
#endif
    // get state pointer
    return list_vp_get_data( netmsg );
}
