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

    // set up IO:

    // SPI_DDR |= (1<<SPI_MOSI)|(1<<SPI_SCK)|(1<<SPI_SS);


    SPSR |= _BV(SPI2X); // set double speed bit
    SPCR = (0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(0<<SPR1)|(0<<SPR0); // SCK / 2
}

void spi_v_release( void ){

    
}

uint32_t spi_u32_get_freq( uint8_t channel ){

    return 8000000;
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

    uint8_t returnvar;
    
    ATOMIC;
    SPDR = data;
    END_ATOMIC;
    
    loop_until_bit_is_set(SPSR,SPIF);
    returnvar = SPDR;
    return returnvar;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

    while( length > 0 ){
        
        // send the data
        SPDR = *data;
        
        // while the data is being sent, update the data pointer,
        // length counter, and then wait for transmission to complete   
        data++;
        length--;
        loop_until_bit_is_set(SPSR,SPIF);
    }
}

// read a block of data from the SPI port.
void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

    // start initial transfer
    SPDR = 0;
    
    while( length > 1 ){
        
        // wait until transfer is complete
        loop_until_bit_is_set(SPSR,SPIF);
        
        // read the data byte
        *data = SPDR;
        
        // start the next transfer
        SPDR = 0;
        
        // decrement length and advance pointer
        length--;
        data++;
    }
    // loop terminates with one byte left
    
    // wait until transfer is complete
    loop_until_bit_is_set(SPSR,SPIF);
    
    // read last data byte
    *data = SPDR;
}
