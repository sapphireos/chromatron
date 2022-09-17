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

#include "cpu.h"
#include "system.h"
#include "spi.h"
#include "hal_spi.h"
#include "hal_io.h"


void spi_v_init( uint8_t channel, uint32_t freq, uint8_t mode ){

	ASSERT( channel < N_SPI_PORTS );

}

void spi_v_release( void ){

	
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	ASSERT( channel < N_SPI_PORTS );

	return 0;
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

	ASSERT( channel < N_SPI_PORTS );

	return 0;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

	
}

void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

	
}

