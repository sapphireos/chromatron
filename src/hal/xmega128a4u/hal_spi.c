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
#include "timers.h"
#include "spi.h"

void spi_v_init( uint8_t channel, uint32_t freq, uint8_t mode ){

    // set TX and XCK pins to output
    SPI_USER_IO_PORT.DIRSET = ( 1 << SPI_USER_SCK_PIN ) | ( 1 << SPI_USER_MOSI_PIN );

    // set RXD to input
    SPI_USER_IO_PORT.DIRCLR = ( 1 << SPI_USER_MISO_PIN );

    // set USART to master SPI mode 0
    SPI_USER_PORT.CTRLC = USART_CMODE_MSPI_gc | ( 0 << UDORD ) | ( 0 << UCPHA );

    // BAUDCTRLA is low byte of BSEL
    // SPI clock = Fper / ( 2 * ( BSEL + 1 ) )
    // Fper will generally be 32 MHz

    // calculate BSEL for frequency
    uint8_t bsel = ( ( CPU_FREQ_PER / freq ) / 2 ) - 1;

    SPI_USER_PORT.BAUDCTRLB = 0;
    SPI_USER_PORT.BAUDCTRLA = bsel;

    // set rate to 16 MHz
    // SPI_USER_PORT.BAUDCTRLA = 0;
    
    // 4 MHz
    // SPI_USER_PORT.BAUDCTRLA = 3;

    // 8 MHz
    // SPI_USER_PORT.BAUDCTRLA = 1;

    // enable TX and RX
    SPI_USER_PORT.CTRLB = USART_RXEN_bm | USART_TXEN_bm;
}

void spi_v_release( void ){

    
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	uint8_t bsel = SPI_USER_PORT.BAUDCTRLA;

	return CPU_FREQ_PER / ( 2 * ( bsel + 1) );
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

    SPI_USER_PORT.DATA = data;

    BUSY_WAIT( ( SPI_USER_PORT.STATUS & USART_TXCIF_bm ) == 0 );
    SPI_USER_PORT.STATUS = USART_TXCIF_bm;

    return SPI_USER_PORT.DATA;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

    uint8_t dummy;

    while( length > 0 ){

        // send the data
        SPI_USER_PORT.DATA = *data;

        // while the data is being sent, update the data pointer,
        // length counter, and then wait for transmission to complete
        data++;
        length--;

        BUSY_WAIT( ( SPI_USER_PORT.STATUS & USART_TXCIF_bm ) == 0 );
        SPI_USER_PORT.STATUS = USART_TXCIF_bm;


        // because there is a FIFO in the UART, we need to read data
        // (and throw away), otherwise the next read will be corrupted.
        dummy = SPI_USER_PORT.DATA;
    }
}

// read a block of data from the SPI port.
void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

    // start initial transfer
    SPI_USER_PORT.DATA = 0;

    while( length > 1 ){

        // wait until transfer is complete
        BUSY_WAIT( ( SPI_USER_PORT.STATUS & USART_TXCIF_bm ) == 0 );
        SPI_USER_PORT.STATUS = USART_TXCIF_bm;

        // read the data byte
        *data = SPI_USER_PORT.DATA;

        // start the next transfer
        SPI_USER_PORT.DATA = 0;

        // decrement length and advance pointer
        length--;
        data++;
    }
    // loop terminates with one byte left

    // wait until transfer is complete
    BUSY_WAIT( ( SPI_USER_PORT.STATUS & USART_TXCIF_bm ) == 0 );
    SPI_USER_PORT.STATUS = USART_TXCIF_bm;

    // read last data byte
    *data = SPI_USER_PORT.DATA;
}
