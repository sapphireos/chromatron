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


#include "system.h"
#include "list.h"
#include "icmp.h"
#include "udp.h"
#include "sockets.h"
#include "keyvalue.h"
#include "fs.h"
#include "config.h"

#include "netmsg.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

static uint32_t netmsg_udp_sent;
static uint32_t netmsg_udp_recv;
static uint32_t netmsg_udp_dropped;

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

#ifndef NETMSG_MAX_RX_Q_SIZE
    #define NETMSG_MAX_RX_Q_SIZE 8
#endif

#include "timers.h"
static uint32_t last_rx_ts;
static uint32_t longest_rx_delta;

static list_t rx_q;


KV_SECTION_META kv_meta_t netmsg_info_kv[] = {
    { CATBUS_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,  &netmsg_udp_sent,    0,   "netmsg_udp_sent" },
    { CATBUS_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,  &netmsg_udp_recv,    0,   "netmsg_udp_recv" },
    { CATBUS_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,  &netmsg_udp_dropped, 0,   "netmsg_udp_dropped" },

    { CATBUS_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,  &longest_rx_delta,    0,   "netmsg_max_rx_delta" },
};

PT_THREAD( netmsg_rx_q_thread( pt_t *pt, void *state ) );

#ifndef ENABLE_COPROCESSOR
static netmsg_port_monitor_t port_monitors[NETMSG_N_PORT_MONITORS];

static netmsg_port_monitor_t* get_port_monitor( netmsg_state_t *state ){

    netmsg_port_monitor_t *ptr = 0;
    netmsg_port_monitor_t *free_ptr = 0;

    for( uint16_t i = 0; i < cnt_of_array(port_monitors); i++ ){

        if( ( free_ptr == 0 ) && ( port_monitors[i].timeout == 0 ) ){

            free_ptr = &port_monitors[i];

            continue;
        }

        if( port_monitors[i].timeout == 0 ){

            continue;
        }

        if( ip_b_addr_compare( port_monitors[i].ipaddr, state->raddr.ipaddr ) &&
            ( port_monitors[i].rport == state->raddr.port ) &&
            ( port_monitors[i].lport == state->laddr.port ) ){

            // match
            ptr = &port_monitors[i];

            break;
        }
    }

    // if existing record not found, but there is a free slot:
    if( ( ptr == 0 ) && ( free_ptr != 0 ) ){

        ptr = free_ptr;

        ptr->timeout = 60;
        ptr->ipaddr = state->raddr.ipaddr;
        ptr->rport = state->raddr.port;
        ptr->lport = state->laddr.port;
    }

    return ptr;
}
#endif

void default_open_close_port( uint8_t protocol, uint16_t port, bool open ){
}

int8_t loopback_i8_get_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask ){

    *subnet = ip_a_addr(127,0,0,0);
    *subnet_mask = ip_a_addr(255,0,0,0);

    return NETMSG_ROUTE_AVAILABLE;
}

int8_t loopback_i8_transmit( netmsg_t msg ){

    netmsg_state_t *state = netmsg_vp_get_state( msg );

    // switch addresses
    sock_addr_t temp_addr = state->raddr;
    state->raddr = state->laddr;
    state->laddr = temp_addr;    

    netmsg_v_receive( msg );

    return NETMSG_TX_OK_NORELEASE;
}

// first entry is loopback interface
ROUTING_TABLE_START routing_table_entry_t route_start = {
    loopback_i8_get_route,
    loopback_i8_transmit,
    default_open_close_port,
    0
};


int8_t loopback_i8_get_local_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return NETMSG_ROUTE_NOT_AVAILABLE;
    }

    cfg_i8_get( CFG_PARAM_IP_ADDRESS, subnet );
    *subnet_mask = ip_a_addr(255,255,255,255);

    return NETMSG_ROUTE_AVAILABLE;
}

int8_t loopback_i8_local_transmit( netmsg_t msg ){

    netmsg_state_t *state = netmsg_vp_get_state( msg );

    // copy remote ip to local ip (which is this node's actual ip)
    // this is done because the laddr ip isn't filled out since 
    // it isn't normally needed (it will usually go out an actual
    // interface, which will attach the local IP to the actual message
    // and the netmsg will be cleared).

    state->laddr.ipaddr = state->raddr.ipaddr;

    // switch ports
    // the addresses will be the same on this route,
    // but the ports need to swap.
    sock_addr_t temp_addr = state->raddr;
    state->raddr = state->laddr;
    state->laddr = temp_addr;    

    netmsg_v_receive( msg );

    return NETMSG_TX_OK_NORELEASE;
}

// second loopback interface, using local IP
ROUTING_TABLE routing_table_entry_t route_local = {
    loopback_i8_get_local_route,
    loopback_i8_local_transmit,
    default_open_close_port,
    0
};

ROUTING_TABLE_END routing_table_entry_t route_end[] = {};

#ifndef ENABLE_COPROCESSOR
static uint32_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    uint32_t ret_val = len;
    uint8_t *data;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:

            data = (uint8_t *)port_monitors;

            memcpy( ptr, &data[pos], len );

            break;

        case FS_VFILE_OP_SIZE:
            ret_val = sizeof(port_monitors);
            break;

        default:
            ret_val = 0;
            break;
    }

    return ret_val;
}
#endif


int8_t _netmsg_i8_receive_sock( netmsg_t netmsg ){

    int8_t status = sock_i8_recv( netmsg );

    if( ( status < 0 ) && 
        ( status != SOCK_STATUS_MCAST_SELF ) ){

        netmsg_udp_dropped++;
    }

    #ifndef ENABLE_COPROCESSOR
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){
    
        // update port monitor
        netmsg_port_monitor_t *port_monitor = get_port_monitor( state );

        if( port_monitor != 0 ){

            port_monitor->timeout = 60;

            if( ( status < 0 ) && 
                ( status != SOCK_STATUS_MCAST_SELF ) && 
                ( port_monitor->dropped < UINT32_MAX ) ){

                port_monitor->dropped++;                    
            }
            else if( port_monitor->rx_count < UINT32_MAX){

                port_monitor->rx_count++;    
            }    
        }
    }   
    #endif

    return status;
}

PT_THREAD( netmsg_rx_q_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, list_u8_count( &rx_q ) == 0 ); 

        netmsg_t netmsg = list_ln_remove_tail( &rx_q );

        int8_t status = _netmsg_i8_receive_sock( netmsg );

        if( status < 0 ){

            if( ( status == SOCK_STATUS_NO_SOCK ) ||
                ( status == SOCK_STATUS_MCAST_SELF ) ){

                netmsg_v_release( netmsg );
            }
            else{

                list_v_insert_head( &rx_q, netmsg );    
            }
        }
        else{

            // socket received, it will keep the data handle
            // and we can release the netmsg

            netmsg_v_release( netmsg );
        }

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}


// initialize netmsg
void netmsg_v_init( void ){

    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        #ifndef ENABLE_COPROCESSOR
        fs_f_create_virtual( PSTR("portinfo"), vfile );
        #endif

        list_v_init( &rx_q );

        thread_t_create( netmsg_rx_q_thread,
                     PSTR("netmsg_rx_q"),
                     0,
                     0 );
    }
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
    msg->header_len = 0;
    msg->data_handle = -1;
    msg->ttl = 0;
    memset( &msg->laddr, 0, sizeof(msg->laddr) );
    memset( &msg->raddr, 0, sizeof(msg->raddr) );

	return n;
}

void netmsg_v_release( netmsg_t netmsg ){

    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    if( state->data_handle >= 0 ){

        mem2_v_free( state->data_handle );
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

        // #ifdef ENABLE_IP

        // // get data
        // uint8_t *data = mem2_vp_get_ptr( state->data_handle );

        // // get headers
        // ip_hdr_t *ip_hdr = (ip_hdr_t *)data;
        // data += sizeof(ip_hdr_t);
        // state->header_len += sizeof(ip_hdr_t);

        // // check the IP header
        // if( ip_b_verify_header( ip_hdr ) != TRUE ){

        //     log_v_debug_P( PSTR("IPv4 header fail: %d.%d.%d.%d -> %d.%d.%d.%d"),
        //         ip_hdr->source_addr.ip3,
        //         ip_hdr->source_addr.ip2,
        //         ip_hdr->source_addr.ip1,
        //         ip_hdr->source_addr.ip0,
        //         ip_hdr->dest_addr.ip3,
        //         ip_hdr->dest_addr.ip2,
        //         ip_hdr->dest_addr.ip1,
        //         ip_hdr->dest_addr.ip0 );

        //     goto clean_up;
        // }

        // // check data length matches ipv4 header
        // if( mem2_u16_get_size( state->data_handle ) != HTONS( ip_hdr->total_length ) ){

        //     log_v_debug_P( PSTR("IPv4 len mismatch: %u != %u"), mem2_u16_get_size( state->data_handle ), HTONS( ip_hdr->total_length ) );

        //     goto clean_up;
        // }

        // bool ip_broadcast = ip_b_check_broadcast( ip_hdr->dest_addr );
        // bool ip_receive = ip_b_check_dest( ip_hdr->dest_addr );

        // // log_v_debug_P( PSTR("IPv4 header: %d.%d.%d.%d -> %d.%d.%d.%d %d"),
        // //     ip_hdr->source_addr.ip3,
        // //     ip_hdr->source_addr.ip2,
        // //     ip_hdr->source_addr.ip1,
        // //     ip_hdr->source_addr.ip0,
        // //     ip_hdr->dest_addr.ip3,
        // //     ip_hdr->dest_addr.ip2,
        // //     ip_hdr->dest_addr.ip1,
        // //     ip_hdr->dest_addr.ip0,
        // //     ip_receive );
        // //


        // // check destination address
        // // check if packet is for this node
        // if( ( ip_receive == TRUE ) || ( ip_broadcast == TRUE ) ){

        //     // set addresses
        //     state->raddr.ipaddr = ip_hdr->source_addr;
        //     state->laddr.ipaddr = ip_hdr->dest_addr;


        //     // does this make sense?  should we have an IP packet instead
        //     // of UDP for this?

        //     // set the netmsg protocol for the message router
        //     if( ip_hdr->protocol == IP_PROTO_ICMP ){

        //         // call the icmp receive handler
        //         icmp_v_recv( netmsg );
        //     }
        //     else if( ip_hdr->protocol == IP_PROTO_UDP ){

        //         udp_header_t *udp_hdr = (udp_header_t *)data;
        //         data += sizeof(udp_header_t);
        //         state->header_len += sizeof(udp_header_t);

        //         // log_v_debug_P( PSTR("header_len %d size: %d"), state->header_len, mem2_u16_get_size( state->data_handle ) );

        //         // check UDP checksum, if checksum is enabled
        //         // if( ( udp_hdr->checksum != 0 ) &&
        //         //     ( udp_u16_checksum( ip_hdr ) != HTONS(udp_hdr->checksum) ) ){
        //         //
        //         //     goto clean_up;
        //         // }

        //         // set ports
        //         state->raddr.port = HTONS(udp_hdr->source_port);
        //         state->laddr.port = HTONS(udp_hdr->dest_port);

        //         int8_t status = sock_i8_recv( netmsg );
        //     }
        //     else{

        //         goto clean_up;
        //     }
        // }
        // else{

        //     goto clean_up;
        // }
        // #else

        // easy mode!

        if( last_rx_ts == 0 ){

            last_rx_ts = tmr_u32_get_system_time_ms();
        }
        else{

            uint32_t delta = tmr_u32_elapsed_time_ms( last_rx_ts );
            last_rx_ts = tmr_u32_get_system_time_ms();   

            if( delta > longest_rx_delta ){

                longest_rx_delta = delta;

                // if( longest_rx_delta > 8000 ){

                //     log_v_debug_P( PSTR("longest delta: %lu"), longest_rx_delta );
                // }
            }
        }

        if( sys_u8_get_mode() != SYS_MODE_SAFE ){

            // attempt to deliver directly to socket
            int8_t status = _netmsg_i8_receive_sock( netmsg );

            if( status < 0 ){

                if( ( status == SOCK_STATUS_NO_SOCK ) ||
                    ( status == SOCK_STATUS_MCAST_SELF ) ){

                    goto clean_up;
                }
                // port buffer is full, attempt to add to rx q
                else if( list_u8_count( &rx_q ) < NETMSG_MAX_RX_Q_SIZE ){

                    list_v_insert_head( &rx_q, netmsg );

                    // make sure we don't release the handle!
                    return;
                }
                else{

                    log_v_info_P( PSTR("netmsg rx q full") );

                    goto clean_up;
                }
            }
        }
        else{

            _netmsg_i8_receive_sock( netmsg );    
        }
        
        // #endif

        netmsg_udp_recv++;
    }
    else{

        ASSERT( FALSE );
    }


// #ifdef ENABLE_IP
clean_up:
// #endif
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

    if( sys_u8_get_mode() != SYS_MODE_SAFE ){
        
        #ifndef ENABLE_COPROCESSOR
        // update port monitor
        netmsg_port_monitor_t *port_monitor = get_port_monitor( state );

        if( port_monitor != 0 ){

            port_monitor->timeout = 60;

            if( port_monitor->tx_count < UINT32_MAX){

                port_monitor->tx_count++;    
            }
        }
        #endif
    }
    

    routing_table_entry_t *ptr = &route_start;
    routing_table_entry_t route;    

    routing_table_entry_t default_route;
    default_route.tx_func = 0;

    // iterate through routes to find a match
    while( ptr < route_end ){

        // load meta data
        memcpy_P( &route, ptr, sizeof(route) );
    
        ip_addr4_t subnet;
        ip_addr4_t subnet_mask;

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

        // #ifdef ENABLE_IP

        // // allocate header 1
        // mem_handle_t ip_hdr_handle = mem2_h_alloc( sizeof(ip_hdr_t) );

        // if( ip_hdr_handle < 0 ){

        //     goto clean_up;
        // }

        // state->header_1_handle = ip_hdr_handle;

        // uint16_t data_size = mem2_u16_get_size( state->data_handle );

        // ip_hdr_t *ip_hdr = mem2_vp_get_ptr( ip_hdr_handle );

        // ip_v_init_header( ip_hdr,
        //                   state->raddr.ipaddr,
        //                   IP_PROTO_ICMP,
        //                   0,
        //                   data_size );

        // #endif
    }
    else if( state->type == NETMSG_TYPE_UDP ){

        // for now, extra header processing selected based on
        // compiler tokens.  eventually should do this based on interface.

        // #ifdef ENABLE_IP

        // mem_handle_t ip_udp_hdr_handle = mem2_h_alloc( sizeof(ip_hdr_t) + sizeof(udp_header_t) );

        // if( ip_udp_hdr_handle < 0 ){

        //     goto clean_up;
        // }

        // state->header_1_handle = ip_udp_hdr_handle;

        // ip_hdr_t *ip_hdr = mem2_vp_get_ptr( ip_udp_hdr_handle );

        // uint16_t data_size = mem2_u16_get_size( state->data_handle );

        // // check if header2 has any data
        // if( state->header_2_handle > 0 ){

        //     data_size += mem2_u16_get_size( state->header_2_handle );
        // }

        // ip_v_init_header( ip_hdr,
        //                   state->raddr.ipaddr,
        //                   IP_PROTO_UDP,
        //                   state->ttl,
        //                   data_size + sizeof(udp_header_t) );

        // udp_header_t *udp_hdr = (udp_header_t *)( ip_hdr + 1 );

        // udp_v_init_header( udp_hdr,
        //                    state->laddr.port,
        //                    state->raddr.port,
        //                    data_size );



        // // TODO!!!! Need to create a checksum routine that works on memory buffers
        // // ASSERT( 0 );

        // // compute checksum
        // // udp_hdr->checksum = HTONS(udp_u16_checksum_netmsg( netmsg ));
        // udp_hdr->checksum = 0;

        // #else

        // // non-IP, easy mode!

        // #endif

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

        sos_assert( 0, file, line );
    }

#else
void *netmsg_vp_get_state( netmsg_t netmsg ){
#endif
    // get state pointer
    return list_vp_get_data( netmsg );
}


void netmsg_v_tick( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    #ifndef ENABLE_COPROCESSOR
    for( uint16_t i = 0; i < cnt_of_array(port_monitors); i++ ){

        if( port_monitors[i].timeout > 0 ){

            port_monitors[i].timeout--;

            // timeout
            if( port_monitors[i].timeout == 0 ){

                memset( &port_monitors[i], 0, sizeof(port_monitors[i]) );
            }
        }
    }
    #endif
}

