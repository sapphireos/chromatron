/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#ifndef __SOCKETS_H
#define __SOCKETS_H

#include "ip.h"
#include "list.h"
#include "memory.h"
#include "netmsg.h"

#define SOCK_SINGLE_BUF

#define SOCK_MEM_BUSY_THRESHOLD         640

#define SOCK_EPHEMERAL_PORT_LOW         49152
#define SOCK_EPHEMERAL_PORT_HIGH        65535

#define SOCK_MAXIMUM_TIMEOUT            60   // in seconds


typedef list_node_t socket_t;


typedef int8_t sock_type_t8;
//#define SOCK_RAW          0x01   // directly over IP - unsupported
//#define SOCK_STREAM       0x02  // TCP not supported
#define SOCK_DGRAM          0x04  // uses UDP


#define SOCK_IS_DGRAM(type) ( type & SOCK_DGRAM )

// UDP states
#define SOCK_UDP_STATE_IDLE             0
#define SOCK_UDP_STATE_RX_WAITING       1
#define SOCK_UDP_STATE_RX_DATA_PENDING  2
#define SOCK_UDP_STATE_RX_DATA_RECEIVED 3
#define SOCK_UDP_STATE_TIMED_OUT        4




// options flags
typedef uint8_t sock_options_t8;
#define SOCK_OPTIONS_TTL_1                      0x01 // set TTL to 1, currently only works on UDP
#define SOCK_OPTIONS_NO_SECURITY                0x02 // disable packet security on wireless
#define SOCK_OPTIONS_SEND_ONLY                  0x04 // socket will not receive data
#define SOCK_OPTIONS_NO_WIRELESS                0x10 // socket will not transmit over the wireless

socket_t sock_s_create( sock_type_t8 type );
void sock_v_release( socket_t sock );
void sock_v_bind( socket_t sock, uint16_t port );
void sock_v_set_options( socket_t sock, sock_options_t8 options );

void sock_v_set_timeout( socket_t sock, uint8_t timeout );

void sock_v_get_raddr( socket_t sock, sock_addr_t *raddr );
// void sock_v_set_raddr( socket_t sock, sock_addr_t *raddr );

uint16_t sock_u16_get_lport( socket_t sock );
int16_t sock_i16_get_bytes_read( socket_t sock );
void *sock_vp_get_data( socket_t sock );
mem_handle_t sock_h_get_data_handle( socket_t sock );

bool sock_b_port_in_use( uint16_t port );
bool sock_b_port_busy( uint16_t port );
bool sock_b_rx_pending( void );

bool sock_b_busy( socket_t sock );

int8_t sock_i8_recvfrom( socket_t sock );
int16_t sock_i16_sendto( socket_t sock, void *buf, uint16_t bufsize, sock_addr_t *raddr );
int16_t sock_i16_sendto_m( socket_t sock, mem_handle_t handle, sock_addr_t *raddr );

void sock_v_recv( netmsg_t netmsg );

void sock_v_init( void );
uint8_t sock_u8_count( void );

void sock_v_process_timeouts( void );

#endif
