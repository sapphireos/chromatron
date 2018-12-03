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

#ifndef _HAL_PIXEL_H
#define _HAL_PIXEL_H

#define PIX0_SPI 			SPI1
#define PIX0_SPI_IRQn 		SPI1_IRQn
#define PIX0_SPI_HANDLER 	SPI1_IRQHandler
#define PIX0_DMA_INSTANCE	DMA2_Stream3
#define PIX0_DMA_IRQ	 	DMA2_Stream3_IRQn
#define PIX0_DMA_HANDLER	DMA2_Stream3_IRQHandler
#define PIX0_DMA_CHANNEL	DMA_CHANNEL_3

#define PIX1_SPI 			SPI2
#define PIX1_SPI_IRQn 		SPI2_IRQn
#define PIX1_SPI_HANDLER 	SPI2_IRQHandler
#define PIX1_DMA_INSTANCE	DMA1_Stream4
#define PIX1_DMA_IRQ	 	DMA1_Stream4_IRQn
#define PIX1_DMA_HANDLER	DMA1_Stream4_IRQHandler
#define PIX1_DMA_CHANNEL	DMA_CHANNEL_0


void hal_pixel_v_init( void );

uint8_t hal_pixel_u8_driver_count( void );
uint16_t hal_pixel_u16_driver_pixels( uint8_t driver );

void hal_pixel_v_transfer_complete_callback( uint8_t driver ) __attribute__((weak));
void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len );

#endif
