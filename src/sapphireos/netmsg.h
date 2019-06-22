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

#ifndef _NETMSG_H
#define _NETMSG_H

#include "list.h"
#include "system.h"
#include "ip.h"

typedef list_node_t netmsg_t;

#include "udp.h"

typedef uint8_t netmsg_flags_t;
#define NETMSG_FLAGS_WCOM_SECURITY_DISABLE  0x01
#define NETMSG_FLAGS_NO_WCOM                0x02
#define NETMSG_FLAGS_RETAIN_DATA            0x04

typedef int8_t netmsg_type_t;
#define NETMSG_TYPE_IP                      0x01
#define NETMSG_TYPE_ICMP                    0x02
#define NETMSG_TYPE_UDP                     0x04

#define NETMSG_ROUTE_NOT_AVAILABLE          -1
#define NETMSG_ROUTE_AVAILABLE              0
#define NETMSG_ROUTE_DEFAULT                1

typedef struct{
	ip_addr_t ipaddr;
	uint16_t port;
} sock_addr_t;

typedef struct{
    netmsg_type_t   type;
    netmsg_flags_t  flags;
    mem_handle_t    header_0_handle;
    mem_handle_t    header_1_handle;
    mem_handle_t    header_2_handle;
    uint8_t         header_len; // length of headers in data_handle
    mem_handle_t    data_handle;
    uint8_t         ttl;
    sock_addr_t     raddr;
    sock_addr_t     laddr;
} netmsg_state_t;

#if defined(__SIM__) || defined(BOOTLOADER)
    #define ROUTING_TABLE
#else
    #define ROUTING_TABLE              __attribute__ ((section (".routing"), used))
#endif

#define ROUTE_FLAGS_ALLOW_GLOBAL_BROADCAST 0x01

typedef int8_t ( *route_i8_get_route )( ip_addr_t *subnet, ip_addr_t *subnet_mask );
typedef int8_t ( *route_i8_transmit_msg )( netmsg_t msg );
typedef void ( *route_v_open_close_port )( uint8_t protocol, uint16_t port, bool open );

typedef struct{
    route_i8_get_route get_route;
    route_i8_transmit_msg tx_func;
    route_v_open_close_port open_close_port;
    uint8_t flags;
} routing_table_entry_t;

extern void ( *netmsg_v_receive_msg )( netmsg_t msg );
// MUST return one of these codes to indicate what to do with the netmsg
#define NETMSG_TX_OK_RELEASE 1
#define NETMSG_TX_OK_NORELEASE -1
#define NETMSG_TX_ERR_RELEASE 2
#define NETMSG_TX_ERR_NORELEASE -2

// protocol in ip.h
void netmsg_v_open_close_port( uint8_t protocol, uint16_t port, bool open );
int8_t netmsg_i8_transmit_msg( netmsg_t msg );

// public api:
void netmsg_v_init( void );

netmsg_t netmsg_nm_create( netmsg_type_t type );
void netmsg_v_release( netmsg_t netmsg );

void netmsg_v_set_flags( netmsg_t netmsg, netmsg_flags_t flags );
netmsg_flags_t netmsg_u8_get_flags( netmsg_t netmsg );


#ifdef ENABLE_EXTENDED_VERIFY
    void *_netmsg_vp_get_state( netmsg_t netmsg, FLASH_STRING_T file, int line );

    #define netmsg_vp_get_state(netmsg)         _netmsg_vp_get_state(netmsg, __SOURCE_FILE__, __LINE__ )
#else
    void *netmsg_vp_get_state( netmsg_t netmsg );
#endif

void netmsg_v_receive( netmsg_t netmsg );

int8_t netmsg_i8_send( netmsg_t netmsg );

#endif
