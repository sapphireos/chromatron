/*
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
 */



#include "cpu.h"

#include "spi.h"

#include "target.h"
#include "system.h"
#include "timers.h"

// this driver is set up to use the USART in master SPI mode


// these bits in USART.CTRLC seem to be missing from the IO header
#define UDORD 2
#define UCPHA 1

static inline void spi_v_init( void );
static inline uint8_t spi_u8_send( uint8_t data ) __attribute__((always_inline));
static inline void spi_v_write_block( const uint8_t *data, uint16_t length ) __attribute__((always_inline));
static inline void spi_v_read_block( uint8_t *data, uint16_t length ) __attribute__((always_inline));


static inline void spi_v_init( void ){

    // set TX and XCK pins to output
    SPI_IO_PORT.DIR |= ( 1 << SPI_SCK_PIN ) | ( 1 << SPI_MOSI_PIN );

    // set RXD to input
    SPI_IO_PORT.DIR &= ~( 1 << SPI_MISO_PIN );

    // set USART to master SPI mode 0
    SPI_PORT.CTRLC = USART_CMODE_MSPI_gc | ( 0 << UDORD ) | ( 0 << UCPHA );

    // BAUDCTRLA is low byte of BSEL
    // SPI clock = Fper / ( 2 * ( BSEL + 1 ) )
    // Fper will generally be 32 MHz

    // set rate to 16 MHz
    SPI_PORT.BAUDCTRLA = 0;
    SPI_PORT.BAUDCTRLB = 0;

    // 4 MHz
    // SPI_PORT.BAUDCTRLA = 3;

    // enable TX and RX
    SPI_PORT.CTRLB = USART_RXEN_bm | USART_TXEN_bm;
}

static inline uint8_t spi_u8_send( uint8_t data ){

    SPI_PORT.DATA = data;

    BUSY_WAIT( ( SPI_PORT.STATUS & USART_TXCIF_bm ) == 0 );
    SPI_PORT.STATUS = USART_TXCIF_bm;

    return SPI_PORT.DATA;
}

static inline void spi_v_write_block( const uint8_t *data, uint16_t length ){

    uint8_t dummy;

    while( length > 0 ){

        // send the data
        SPI_PORT.DATA = *data;

        // while the data is being sent, update the data pointer,
        // length counter, and then wait for transmission to complete
        data++;
        length--;

        BUSY_WAIT( ( SPI_PORT.STATUS & USART_TXCIF_bm ) == 0 );
        SPI_PORT.STATUS = USART_TXCIF_bm;


        // because there is a FIFO in the UART, we need to read data
        // (and throw away), otherwise the next read will be corrupted.
        dummy = SPI_PORT.DATA;
    }
}

// read a block of data from the SPI port.
static inline void spi_v_read_block( uint8_t *data, uint16_t length ){

    // start initial transfer
    SPI_PORT.DATA = 0;

    while( length > 1 ){

        // wait until transfer is complete
        BUSY_WAIT( ( SPI_PORT.STATUS & USART_TXCIF_bm ) == 0 );
        SPI_PORT.STATUS = USART_TXCIF_bm;

        // read the data byte
        *data = SPI_PORT.DATA;

        // start the next transfer
        SPI_PORT.DATA = 0;

        // decrement length and advance pointer
        length--;
        data++;
    }
    // loop terminates with one byte left

    // wait until transfer is complete
    BUSY_WAIT( ( SPI_PORT.STATUS & USART_TXCIF_bm ) == 0 );
    SPI_PORT.STATUS = USART_TXCIF_bm;

    // read last data byte
    *data = SPI_PORT.DATA;
}
