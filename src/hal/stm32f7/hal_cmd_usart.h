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

#ifndef HAL_CMD_USART_H
#define HAL_CMD_USART_H

#include "usart_bauds.h"
#include "cmd_usart.h"

#include "udp.h"

#define HAL_CMD_USART 				USART1
#define HAL_CMD_USART_RX_BUF_SIZE 	128

#define CMD_USART_MAX_PACKET_LEN    548

#define CMD_USART_TIMEOUT_MS        250

#define CMD_USART_VERSION           0x02
#define CMD_USART_UDP_SOF           0x85
#define CMD_USART_UDP_ACK           0x97
#define CMD_USART_UDP_NAK           0x14

typedef struct  __attribute__((packed)){
    uint16_t lport;
    uint16_t rport;
    uint16_t data_len;
    uint16_t header_crc;
} cmd_usart_udp_header_t;

#endif
