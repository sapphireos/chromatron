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

#ifndef _HAL_PIXEL_H
#define _HAL_PIXEL_H

#define PIX0_SPI 			SPI1
#define PIX0_SPI_IRQn 		SPI1_IRQn
#define PIX0_SPI_HANDLER 	SPI1_IRQHandler
#define PIX0_DMA_INSTANCE	DMA1_Stream2
#define PIX0_DMA_IRQ	 	DMA1_Stream2_IRQn
#define PIX0_DMA_HANDLER	DMA1_Stream2_IRQHandler
#define PIX0_DMA_REQUEST	DMA_REQUEST_SPI1_TX

#ifdef BOARD_CHROMATRONX
#define PIX1_SPI 			SPI2
#define PIX1_SPI_IRQn 		SPI2_IRQn
#define PIX1_SPI_HANDLER 	SPI2_IRQHandler
#define PIX1_DMA_INSTANCE	DMA1_Stream3
#define PIX1_DMA_IRQ	 	DMA1_Stream3_IRQn
#define PIX1_DMA_HANDLER	DMA1_Stream3_IRQHandler
#define PIX1_DMA_REQUEST	DMA_REQUEST_SPI2_TX

#define PIX2_SPI 			SPI3
#define PIX2_SPI_IRQn 		SPI3_IRQn
#define PIX2_SPI_HANDLER 	SPI3_IRQHandler
#define PIX2_DMA_INSTANCE	DMA1_Stream4
#define PIX2_DMA_IRQ	 	DMA1_Stream4_IRQn
#define PIX2_DMA_HANDLER	DMA1_Stream4_IRQHandler
#define PIX2_DMA_REQUEST	DMA_REQUEST_SPI3_TX

#define PIX3_SPI 			SPI4
#define PIX3_SPI_IRQn 		SPI4_IRQn
#define PIX3_SPI_HANDLER 	SPI4_IRQHandler
#define PIX3_DMA_INSTANCE	DMA1_Stream5
#define PIX3_DMA_IRQ	 	DMA1_Stream5_IRQn
#define PIX3_DMA_HANDLER	DMA1_Stream5_IRQHandler
#define PIX3_DMA_REQUEST	DMA_REQUEST_SPI4_TX

#define PIX4_SPI 			SPI5
#define PIX4_SPI_IRQn 		SPI5_IRQn
#define PIX4_SPI_HANDLER 	SPI5_IRQHandler
#define PIX4_DMA_INSTANCE	DMA1_Stream6
#define PIX4_DMA_IRQ	 	DMA1_Stream6_IRQn
#define PIX4_DMA_HANDLER	DMA1_Stream6_IRQHandler
#define PIX4_DMA_REQUEST	DMA_REQUEST_SPI5_TX

// SPI 6 is on BDMA (domain 3)
// need to fix this so we can use BDMA
// or just not use PIX5...
#define PIX5_SPI 			SPI6
#define PIX5_SPI_IRQn 		SPI6_IRQn
#define PIX5_SPI_HANDLER 	SPI6_IRQHandler
#define PIX5_DMA_INSTANCE	DMA2_Stream2
#define PIX5_DMA_IRQ	 	DMA2_Stream2_IRQn
#define PIX5_DMA_HANDLER	DMA2_Stream2_IRQHandler
#define PIX5_DMA_REQUEST	DMA_REQUEST_SPI6_TX


// #define PIX7_USART 			USART2
// #define PIX7_USART_IRQn 	USART2_IRQn
// #define PIX7_USART_HANDLER 	USART2_IRQHandler
// #define PIX7_DMA_INSTANCE	DMA2_Stream4
// #define PIX7_DMA_IRQ	 	DMA2_Stream4_IRQn
// #define PIX7_DMA_HANDLER	DMA2_Stream4_IRQHandler
// #define PIX7_DMA_REQUEST	DMA_REQUEST_USART2_TX

// #define PIX8_UART 			UART4
// #define PIX8_UART_IRQn 		UART4_IRQn
// #define PIX8_UART_HANDLER 	UART4_IRQHandler
// #define PIX8_DMA_INSTANCE	DMA2_Stream4
// #define PIX8_DMA_IRQ	 	DMA2_Stream4_IRQn
// #define PIX8_DMA_HANDLER	DMA2_Stream4_IRQHandler
// #define PIX8_DMA_REQUEST	DMA_REQUEST_UART4_TX

// #define PIX9_UART 			UART5
// #define PIX9_UART_IRQn 		UART5_IRQn
// #define PIX9_UART_HANDLER 	UART5_IRQHandler
// #define PIX9_DMA_INSTANCE	DMA2_Stream5
// #define PIX9_DMA_IRQ	 	DMA2_Stream5_IRQn
// #define PIX9_DMA_HANDLER	DMA2_Stream5_IRQHandler
// #define PIX9_DMA_REQUEST	DMA_REQUEST_UART5_TX

void hal_pixel_v_transmit_pix5( void );

#endif


void hal_pixel_v_init( void );

uint8_t hal_pixel_u8_driver_count( void );
uint16_t hal_pixel_u16_driver_pixels( uint8_t driver );
uint16_t hal_pixel_u16_driver_offset( uint8_t driver );
uint16_t hal_pixel_u16_get_pix_count( void );

void hal_pixel_v_transfer_complete_callback( uint8_t driver ) __attribute__((weak));
void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len );

#endif
