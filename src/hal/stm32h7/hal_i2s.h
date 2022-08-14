/*
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
 */

#ifndef _HAL_I2S_H_
#define _HAL_I2S_H_

#include "i2s.h"

#define I2S_SIGNAL 			SIGNAL_SYS_7

#define I2S_BUF_SIZE		512
 
#define I2S 				SPI1
#define I2S_SPI_IRQn 		SPI1_IRQn
#define I2S_SPI_HANDLER 	SPI1_IRQHandler
#define I2S_DMA_INSTANCE	DMA1_Stream7
#define I2S_DMA_IRQ	 		DMA1_Stream7_IRQn
#define I2S_DMA_HANDLER		DMA1_Stream7_IRQHandler
#define I2S_DMA_REQUEST		DMA_REQUEST_SPI1_RX

#define hal_i2s_v_init i2s_v_init
#define hal_i2s_v_start i2s_v_start

void hal_i2s_v_init( void );
void hal_i2s_v_start( uint16_t sample_rate, uint8_t sample_bits );

#endif

