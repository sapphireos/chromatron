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

#ifndef _ESP8266_H
#define _ESP8266_H

#include "netmsg.h"
#include "threading.h"
#include "wifi_cmd.h"



#define WIFI_STATE_ERROR        -2
#define WIFI_STATE_BOOT         -1
#define WIFI_STATE_UNKNOWN      0
#define WIFI_STATE_ALIVE        1



#define WIFI_USART              USARTE0
#define WIFI_USART_DMA_TRIG     DMA_CH_TRIGSRC_USARTE0_RXC_gc
#define WIFI_DMA_CH             CH2
#define WIFI_DMA_CHTRNIF        DMA_CH2TRNIF_bm
#define WIFI_DMA_CHERRIF        DMA_CH2ERRIF_bm
#define WIFI_DMA_IRQ_VECTOR     DMA_CH2_vect

#define WIFI_USART_TXD_PORT     PORTE
#define WIFI_USART_TXD_PIN      3
#define WIFI_USART_RXD_PORT     PORTE
#define WIFI_USART_RXD_PIN      2
#define WIFI_USART_XCK_PORT     PORTE
#define WIFI_USART_XCK_PIN      1
#define WIFI_USART_XCK_PINCTRL  PIN1CTRL

#define WIFI_IRQ_VECTOR         PORTA_INT0_vect

#define WIFI_SIGNAL 			SIGNAL_SYS_3


#define WIFI_PD_PORT            PORTE
#define WIFI_PD_PIN             0

#define WIFI_BOOT_PORT          PORTA
#define WIFI_BOOT_PIN           5
#define WIFI_BOOT_PINCTRL       PIN5CTRL

#define WIFI_SS_PORT            PORTA
#define WIFI_SS_PIN             6
#define WIFI_SS_PINCTRL         PIN6CTRL


#define WIFI_SPI_READ_STATUS    0x04
#define WIFI_SPI_WRITE_STATUS   0x01
#define WIFI_SPI_READ_DATA      0x03
#define WIFI_SPI_WRITE_DATA     0x02


#define WIFI_AP_MIN_PASS_LEN    8


#define WIFI_LOADER_MAX_TRIES   8


void wifi_v_init( void );
bool wifi_b_connected( void );
bool wifi_b_ap_mode( void );
bool wifi_b_ap_mode_enabled( void );
bool wifi_b_attached( void );
int8_t wifi_i8_get_status( void );
uint32_t wifi_u32_get_received( void );

int8_t wifi_i8_send_udp( netmsg_t netmsg );

bool wifi_b_running( void );

int8_t wifi_i8_send_msg( uint8_t data_id, uint8_t *data, uint8_t len );
int8_t wifi_i8_send_msg_blocking( uint8_t data_id, uint8_t *data, uint8_t len );
// int8_t wifi_i8_send_msg_response( 
//     uint8_t data_id, 
//     uint8_t *data, 
//     uint8_t len,
//     uint8_t *response,
//     uint8_t response_len );

bool wifi_b_comm_ready( void );
bool wifi_b_wait_comm_ready( void );


extern int8_t wifi_i8_msg_handler( uint8_t data_id, uint8_t *data, uint8_t len ) __attribute__((weak));

#endif
