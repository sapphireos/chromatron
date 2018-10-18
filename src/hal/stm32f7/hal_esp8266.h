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

#ifndef _HAL_ESP8266_H
#define _HAL_ESP8266_H

#include "system.h"
#include "wifi_cmd.h"
#include "hal_usart.h"

#define WIFI_TIMER 				TIM3
#define WIFI_TIMER_ISR  		TIM3_IRQHandler

#define WIFI_UART_RX_BUF_SIZE   WIFI_MAIN_BUF_LEN

#define WIFI_DMA 				DMA1_Stream0 
#define WIFI_DMA_STREAM			0

#define WIFI_USART              USART6
// #define WIFI_USART_DMA_TRIG     DMA_CH_TRIGSRC_USARTE0_RXC_gc
// #define WIFI_DMA_CH             CH2
// #define WIFI_DMA_CHTRNIF        DMA_CH2TRNIF_bm
// #define WIFI_DMA_CHERRIF        DMA_CH2ERRIF_bm
// #define WIFI_DMA_IRQ_VECTOR     DMA_CH2_vect

// #define WIFI_USART_TXD_PORT     PORTE
// #define WIFI_USART_TXD_PIN      3
// #define WIFI_USART_RXD_PORT     PORTE
// #define WIFI_USART_RXD_PIN      2
// #define WIFI_USART_XCK_PORT     PORTE
// #define WIFI_USART_XCK_PIN      1
// #define WIFI_USART_XCK_PINCTRL  PIN1CTRL

// #define WIFI_IRQ_VECTOR         PORTA_INT0_vect

// #define WIFI_PD_PORT            PORTE
// #define WIFI_PD_PIN             0

// #define WIFI_BOOT_PORT          PORTA
// #define WIFI_BOOT_PIN           5
// #define WIFI_BOOT_PINCTRL       PIN5CTRL

// #define WIFI_SS_PORT            PORTA
// #define WIFI_SS_PIN             6
// #define WIFI_SS_PINCTRL         PIN6CTRL



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

void hal_wifi_v_usart_set_baud( baud_t8 baud );

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


