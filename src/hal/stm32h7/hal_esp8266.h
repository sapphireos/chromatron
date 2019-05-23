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

#ifndef _HAL_ESP8266_H
#define _HAL_ESP8266_H

#include "system.h"
#include "wifi_cmd.h"
#include "hal_usart.h"



#define WIFI_UART_RX_BUF_SIZE   WIFI_MAIN_BUF_LEN

#define WIFI_DMA 				DMA1_Stream1
#define WIFI_DMA_IRQ 			DMA1_Stream1_IRQn
#ifdef BOARD_CHROMATRONX
#define WIFI_DMA_REQUEST		DMA_REQUEST_UART8_RX
#define WIFI_USART              UART8
#else
#define WIFI_DMA_REQUEST		DMA_REQUEST_UART4_RX
#define WIFI_USART              UART4
#endif

#ifdef DEBUG
#define WIFI_THREAD_TIMEOUT 	5000 // trace_prints will stall threads
#else
#define WIFI_THREAD_TIMEOUT 	500
#endif

void hal_wifi_v_init( void );

void hal_wifi_v_reset( void );

uint8_t *hal_wifi_u8p_get_rx_dma_buf_ptr( void );
uint8_t *hal_wifi_u8p_get_rx_buf_ptr( void );

void hal_wifi_v_usart_send_char( uint8_t b );
void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len );
int16_t hal_wifi_i16_usart_get_char( void );
void hal_wifi_v_usart_flush( void );

uint16_t hal_wifi_u16_dma_rx_bytes( void );
void hal_wifi_v_disable_rx_dma( void );
void hal_wifi_v_enable_rx_dma( bool irq );

void hal_wifi_v_usart_set_baud( baud_t baud );

void hal_wifi_v_reset_rx_buffer( void );
void hal_wifi_v_clear_rx_buffer( void );
void hal_wifi_v_release_rx_buffer( void );

void hal_wifi_v_reset_control_byte( void );
void hal_wifi_v_reset_comm( void );
void hal_wifi_v_set_rx_ready( void );
void hal_wifi_v_disable_irq( void );
void hal_wifi_v_enable_irq( void );

uint8_t hal_wifi_u8_get_control_byte( void );
int16_t hal_wifi_i16_rx_data_received( void );

void hal_wifi_v_clear_rx_ready( void );
bool hal_wifi_b_comm_ready( void );

uint32_t hal_wifi_u32_get_max_ready_wait( void );
uint32_t hal_wifi_u32_get_rx_bytes( void );
uint32_t hal_wifi_u32_get_tx_bytes( void );

void hal_wifi_v_enter_boot_mode( void );
void hal_wifi_v_enter_normal_mode( void );

#endif


