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
#include "hal_pixel.h"

#include "driver/spi_master.h"

static spi_device_handle_t spi[N_SPI_PORTS];
static spi_device_interface_config_t devcfg[N_SPI_PORTS];
static bool initialized[N_SPI_PORTS]; // thanks IDF 4.4, I really wanted to waste 2 bytes on this.

spi_device_handle_t hal_spi_s_get_handle( void ){

    return spi[0];
}

void spi_v_init( uint8_t channel, uint32_t freq, uint8_t mode ){

	ASSERT( channel < N_SPI_PORTS );
	ASSERT( freq > 0 );

    if( spi[channel] != 0 ){

		spi_bus_remove_device( spi[channel] );
		spi[channel] = 0;
	}

    
    spi_bus_config_t buscfg = {
            .quadwp_io_num      = -1,
            .quadhd_io_num      = -1,
            .max_transfer_sz    = 256,
            .intr_flags         = ESP_INTR_FLAG_IRAM,
        };

    uint8_t port;

    if( channel == PIXEL_SPI_CHANNEL ){

        port = HAL_PIXEL_PORT;

        // io_v_set_mode( HAL_PIXEL_MISO, IO_MODE_INPUT );
        io_v_set_mode( HAL_PIXEL_MOSI, IO_MODE_OUTPUT );
        io_v_set_mode( HAL_PIXEL_SCK, IO_MODE_OUTPUT );

        buscfg.miso_io_num = -1; // MISO is not used on the pixel bus
        buscfg.mosi_io_num = hal_io_i32_get_gpio_num( HAL_PIXEL_MOSI );
        buscfg.sclk_io_num = hal_io_i32_get_gpio_num( HAL_PIXEL_SCK );
        buscfg.max_transfer_sz = PIXEL_BUF_SIZE; // increase max transfer size to pixel bufs
    }
    else{

        port = HAL_SPI_PORT;   
    
        io_v_set_mode( HAL_SPI_MISO, IO_MODE_INPUT );
        io_v_set_mode( HAL_SPI_MOSI, IO_MODE_OUTPUT );
        io_v_set_mode( HAL_SPI_SCK, IO_MODE_OUTPUT );

        buscfg.miso_io_num = hal_io_i32_get_gpio_num( HAL_SPI_MISO );
        buscfg.mosi_io_num = hal_io_i32_get_gpio_num( HAL_SPI_MOSI );
        buscfg.sclk_io_num = hal_io_i32_get_gpio_num( HAL_SPI_SCK );
    }

    if( initialized[channel] ){

        spi_bus_free( port );    
        initialized[channel] = FALSE;
    }

    if( channel == PIXEL_SPI_CHANNEL ){
    
	   ESP_ERROR_CHECK(spi_bus_initialize( port, &buscfg, 1 )); // DMA channel 1
    }
    else{

        ESP_ERROR_CHECK(spi_bus_initialize( port, &buscfg, 0 )); // no DMA
    }

    initialized[channel] = TRUE;

	
    devcfg[channel].clock_speed_hz   = freq;
    devcfg[channel].mode             = mode;                            
    devcfg[channel].spics_io_num     = -1;
    devcfg[channel].queue_size       = 7;       

    ESP_ERROR_CHECK(spi_bus_add_device( port, &devcfg[channel], &spi[channel] ));   
}

void spi_v_release( void ){

	// tristate all IO
	// io_v_set_mode( HAL_PIXEL_MISO, IO_MODE_INPUT );
    io_v_set_mode( HAL_PIXEL_MOSI, IO_MODE_INPUT );
    io_v_set_mode( HAL_PIXEL_SCK, IO_MODE_INPUT );
}

void spi_v_set_freq( uint8_t channel, uint32_t freq ){

    ASSERT( channel < N_SPI_PORTS );

    uint8_t port;

    if( channel == PIXEL_SPI_CHANNEL ){

        port = HAL_PIXEL_PORT;

    }
    else{

        port = HAL_SPI_PORT;   
    }

    if( spi[channel] != 0 ){

        spi_bus_remove_device( spi[channel] );
        spi[channel] = 0;
    }

    devcfg[channel].clock_speed_hz = freq;

    ESP_ERROR_CHECK(spi_bus_add_device( port, &devcfg[channel], &spi[channel] ));   
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	ASSERT( channel < N_SPI_PORTS );

	return devcfg[channel].clock_speed_hz;
}

void spi_v_set_mode( uint8_t channel, uint8_t mode ){

    if( spi[channel] != 0 ){

        spi_bus_remove_device( spi[channel] );
        spi[channel] = 0;
    }

    devcfg[channel].mode = mode;

    uint8_t port;

    if( channel == PIXEL_SPI_CHANNEL ){

        port = HAL_PIXEL_PORT;

    }
    else{

        port = HAL_SPI_PORT;   
    }

    ESP_ERROR_CHECK(spi_bus_add_device( port, &devcfg[channel], &spi[channel] ));   
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

	ASSERT( channel < N_SPI_PORTS );

	uint8_t rx_data = 0;

	spi_transaction_t transaction = { 0 };
	transaction.length = 8; // note lengths are in bits, not bytes!
	transaction.rxlength = 8;
	transaction.tx_buffer = &data;
	transaction.rx_buffer = &rx_data;

	spi_device_polling_transmit( spi[channel], &transaction );

	return rx_data;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

	spi_transaction_t transaction = { 0 };
	transaction.length = (uint32_t)length * 8;
	transaction.rxlength = 0;
	transaction.tx_buffer = data;
	transaction.rx_buffer = 0;

	spi_device_polling_transmit( spi[channel], &transaction );
}

void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );
	
	spi_transaction_t transaction = { 0 };
	transaction.length = (uint32_t)length * 8;
	transaction.rxlength = (uint32_t)length * 8;
	transaction.tx_buffer = 0;
	transaction.rx_buffer = data;

	spi_device_polling_transmit( spi[channel], &transaction );
}

