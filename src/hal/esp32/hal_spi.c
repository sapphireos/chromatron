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

#include "cpu.h"
#include "system.h"
#include "spi.h"
#include "hal_spi.h"
#include "hal_io.h"

#include "driver/spi_master.h"

static spi_device_handle_t spi;
static spi_device_interface_config_t devcfg;

void spi_v_init( uint8_t channel, uint32_t freq, uint8_t mode ){

	ASSERT( channel < N_SPI_PORTS );
	ASSERT( freq > 0 );

    io_v_set_mode( HAL_SPI_MISO, IO_MODE_INPUT );
    io_v_set_mode( HAL_SPI_MOSI, IO_MODE_OUTPUT );
    io_v_set_mode( HAL_SPI_SCK, IO_MODE_OUTPUT );

	if( spi != 0 ){

		spi_bus_remove_device( spi );
		spi = 0;
	}

	spi_bus_free( HAL_SPI_PORT );

	spi_bus_config_t buscfg = {
        .miso_io_num 		= hal_io_i32_get_gpio_num( HAL_SPI_MISO ),
        .mosi_io_num 		= hal_io_i32_get_gpio_num( HAL_SPI_MOSI ),
        .sclk_io_num 		= hal_io_i32_get_gpio_num( HAL_SPI_SCK ),
        .quadwp_io_num 		= -1,
        .quadhd_io_num 		= -1,
        .max_transfer_sz 	= 0, // sets default
    };

	ESP_ERROR_CHECK(spi_bus_initialize( HAL_SPI_PORT, &buscfg, 1 )); // DMA channel 1

	
    devcfg.clock_speed_hz   = freq;
    devcfg.mode             = mode;                            
    devcfg.spics_io_num     = -1;
    devcfg.queue_size       = 1;       

    ESP_ERROR_CHECK(spi_bus_add_device( HAL_SPI_PORT, &devcfg, &spi ));   
}

void spi_v_release( void ){

	// tristate all IO
	io_v_set_mode( HAL_SPI_MISO, IO_MODE_INPUT );
    io_v_set_mode( HAL_SPI_MOSI, IO_MODE_INPUT );
    io_v_set_mode( HAL_SPI_SCK, IO_MODE_INPUT );
}

void spi_v_set_freq( uint8_t channel, uint32_t freq ){

    ASSERT( channel < N_SPI_PORTS );

    if( spi != 0 ){

        spi_bus_remove_device( spi );
        spi = 0;
    }

    devcfg.clock_speed_hz = freq;

    ESP_ERROR_CHECK(spi_bus_add_device( HAL_SPI_PORT, &devcfg, &spi ));   
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	ASSERT( channel < N_SPI_PORTS );

	return devcfg.clock_speed_hz;
}

void spi_v_set_mode( uint8_t channel, uint8_t mode ){

    if( spi != 0 ){

        spi_bus_remove_device( spi );
        spi = 0;
    }

    devcfg.mode = mode;

    ESP_ERROR_CHECK(spi_bus_add_device( HAL_SPI_PORT, &devcfg, &spi ));   
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

	ASSERT( channel < N_SPI_PORTS );

	uint8_t rx_data = 0;

	spi_transaction_t transaction = { 0 };
	transaction.length = 8; // note lengths are in bits, not bytes!
	transaction.rxlength = 8;
	transaction.tx_buffer = &data;
	transaction.rx_buffer = &rx_data;

	spi_device_polling_transmit( spi, &transaction );

	return rx_data;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

	spi_transaction_t transaction = { 0 };
	transaction.length = (uint32_t)length * 8;
	transaction.rxlength = 0;
	transaction.tx_buffer = data;
	transaction.rx_buffer = 0;

	spi_device_polling_transmit( spi, &transaction );
}

void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );
	
	spi_transaction_t transaction = { 0 };
	transaction.length = 0;
	transaction.rxlength = (uint32_t)length * 8;
	transaction.tx_buffer = 0;
	transaction.rx_buffer = data;

	spi_device_polling_transmit( spi, &transaction );
}

