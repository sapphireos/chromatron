/*
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
 */


#ifndef _HAL_SPI_H
#define _HAL_SPI_H

#include "driver/spi_master.h"
#include "hal_io.h"

#define N_SPI_PORTS 1

#define HAL_SPI_PORT 	HSPI_HOST // HSPI is SPI2 on the ESP32


#define HAL_SPI_MISO IO_PIN_19_MISO
#define HAL_SPI_MOSI IO_PIN_18_MOSI
#define HAL_SPI_SCK  IO_PIN_5_SCK


#define ESP32_MAX_SPI_XFER  SPI_MAX_DMA_LEN

spi_device_handle_t hal_spi_s_get_handle( void );

#endif