// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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


#ifndef _CMD_USART_H
#define _CMD_USART_H

#include "usart_bauds.h"

#define CMD_USART_MAX_PACKET_LEN    548

#define CMD_USART_TIMEOUT_MS        250

#define CMD_USART_VERSION           0x02
#define CMD_USART_UDP_SOF           0x85
#define CMD_USART_UDP_ACK           0x97
#define CMD_USART_UDP_NAK           0x14

// must be 8 bytes to fit in endpoint size!
typedef struct __attribute__((packed)){
    uint16_t lport;
    uint16_t rport;
    uint16_t data_len;
    uint16_t header_crc;
} cmd_usart_udp_header_t;

void cmd_usart_v_init( void );

void cmd_usart_v_set_baud( baud_t baud );

bool cmd_usart_b_received_char( void );

void cmd_usart_v_send_char( uint8_t data );
void cmd_usart_v_send_data( const uint8_t *data, uint16_t len );

int16_t cmd_usart_i16_get_char( void );
uint16_t cmd_usart_u16_rx_size( void );
uint8_t cmd_usart_u8_get_data( uint8_t *data, uint8_t len );

void cmd_usart_v_flush( void );

#endif