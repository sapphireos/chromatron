/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#include "driver/spi_master.h"


void spi_v_init( uint8_t channel, uint32_t freq, uint8_t mode ){

	ASSERT( channel < N_SPI_PORTS );

	io_v_set_mode( IO_MODE_INPUT, HAL_SPI_MISO );
	io_v_set_mode( IO_MODE_OUTPUT, HAL_SPI_MOSI );
	io_v_set_mode( IO_MODE_OUTPUT, HAL_SPI_SCK );

	spi_bus_config_t buscfg = {
        .miso_io_num 		= PIN_NUM_MISO,
        .mosi_io_num 		= PIN_NUM_MOSI,
        .sclk_io_num 		= PIN_NUM_CLK,
        .quadwp_io_num 		= -1,
        .quadhd_io_num 		= -1,
        .max_transfer_sz 	= 0, // sets default
    };

	ESP_ERROR_CHECK(spi_bus_initialize( HAL_SPI_PORT, &buscfg, 1 ));


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

