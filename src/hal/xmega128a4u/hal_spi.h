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

#ifndef _HAL_SPI_H
#define _HAL_SPI_H

#include "cpu.h"
#include "system.h"

// this driver is set up to use the USART in master SPI mode

// usartc1 spi
#define SPI_USER_MOSI_PORT PORTC
#define SPI_USER_MISO_PORT PORTC
#define SPI_USER_SCK_PIN 7
#define SPI_USER_MISO_PIN 6
#define SPI_USER_MOSI_PIN 5
#define SPI_USER_IO_PORT PORTC
#define SPI_USER_PORT USARTC1


// these bits in USART.CTRLC seem to be missing from the IO header
#define UDORD 2
#define UCPHA 1


#endif
