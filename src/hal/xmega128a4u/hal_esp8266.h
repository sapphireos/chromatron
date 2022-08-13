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

#ifndef _HAL_ESP8266_H
#define _HAL_ESP8266_H

#include "system.h"
#include "io.h"
#include "wifi_cmd.h"
#include "hal_usart.h"

#define WIFI_UART_BUF_SIZE      255

#define WIFI_USART_CH           USARTE0

#define WIFI_TIMER              TCD1
#define WIFI_TIMER_ISR          TCD1_OVF_vect

// ESP8266 GPIO12
#define WIFI_USART_TXD_PORT     PORTE
#define WIFI_USART_TXD_PIN      3

// ESP8266 GPIO13
#define WIFI_USART_RXD_PORT     PORTE
#define WIFI_USART_RXD_PIN      2


// ESP8266 GPIO14
#define WIFI_IRQ_PIN      		1
#define WIFI_IRQ_PORT      		PORTE
#define WIFI_IRQ_PINCTRL  		PIN1CTRL

#define WIFI_PD_PORT            PORTE
#define WIFI_PD_PIN             0

// ESP8266 GPIO0 (BOOT))
#define WIFI_BOOT_PORT          PORTA
#define WIFI_BOOT_PIN           5
#define WIFI_BOOT_PINCTRL       PIN5CTRL

// ESP8266 GPIO15
#define WIFI_CTS_PORT            PORTA
#define WIFI_CTS_PIN             6
#define WIFI_CTS_PINCTRL         PIN6CTRL

// DMA
#define WIFI_DMA_CH              CH2
#define WIFI_DMA_CH_TRNIF_FLAG   DMA_CH2TRNIF_bm
#define WIFI_DMA_CH_vect         DMA_CH2_vect
#define WIFI_USART_DMA_TRIG      DMA_CH_TRIGSRC_USARTE0_RXC_gc

void hal_wifi_v_init( void );

void hal_wifi_v_reset( void );

void hal_wifi_v_usart_send_char( uint8_t b );
void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len );
int16_t hal_wifi_i16_usart_get_char( void );
int16_t hal_wifi_i16_usart_get_char_timeout( uint32_t timeout );
bool hal_wifi_b_usart_rx_available( void );
int8_t hal_wifi_i8_usart_receive( uint8_t *buf, uint16_t len, uint32_t timeout );

void hal_wifi_v_usart_flush( void );

void hal_wifi_v_usart_set_baud( baud_t baud );

int16_t hal_wifi_i16_rx_data_received( void );

uint32_t hal_wifi_u32_get_rx_bytes( void );
uint32_t hal_wifi_u32_get_tx_bytes( void );

bool hal_wifi_b_read_irq( void );
void hal_wifi_v_set_cts( bool value );

void hal_wifi_v_enter_boot_mode( void );
void hal_wifi_v_enter_normal_mode( void );
#endif


