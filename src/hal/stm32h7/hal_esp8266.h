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

#ifndef _HAL_ESP8266_H
#define _HAL_ESP8266_H

#include "system.h"
#include "wifi_cmd.h"
#include "hal_usart.h"

#define WIFI_RESET_DELAY_MS 	20

#ifdef BOARD_CHROMATRONX
#define WIFI_USART              UART8
#else
#ifdef BOARD_NUCLEAR
#define WIFI_USART              UART7
#else
#define WIFI_USART              UART4
#endif
#endif

#ifdef DEBUG
#define WIFI_THREAD_TIMEOUT 	5000 // trace_prints will stall threads
#else
#define WIFI_THREAD_TIMEOUT 	500
#endif

void hal_esp8266_v_init( void );

void hal_wifi_v_reset( void );

void hal_wifi_v_usart_send_char( uint8_t b );
void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len );
int16_t hal_wifi_i16_usart_get_char( void );
int16_t hal_wifi_i16_usart_get_char_timeout( uint32_t timeout );
bool hal_wifi_b_usart_rx_available( void );
int8_t hal_wifi_i8_usart_receive( uint8_t *buf, uint16_t len, uint32_t timeout );

void hal_wifi_v_usart_flush( void );

void hal_wifi_v_usart_set_baud( baud_t baud );

uint32_t hal_wifi_u32_get_rx_bytes( void );
uint32_t hal_wifi_u32_get_tx_bytes( void );

bool hal_wifi_b_read_irq( void );

void hal_wifi_v_enter_boot_mode( void );
void hal_wifi_v_enter_normal_mode( void );

#endif


