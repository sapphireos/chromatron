/* 
 * <license>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This file is part of the Sapphire Operating System
 *
 * Copyright 2013 Sapphire Open Systems
 *
 * </license>
 */
 

#include "cpu.h"

#include "spi.h"

#include "target.h"
#include "system.h"

#define SPIMode = 0

void spi_v_init(){

	SPI_DDR |= (1<<SPI_MOSI)|(1<<SPI_SCK)|(1<<SPI_SS);
	SPSR |= _BV(SPI2X); // set double speed bit
	SPCR = (0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(0<<SPR1)|(0<<SPR0); // SCK / 2
	//SPCR = (0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(1<<SPR1)|(1<<SPR0); // SCK / 2
}

uint8_t spi_u8_send(uint8_t data){
	uint8_t returnvar;
	
	ATOMIC;
	SPDR = data;
	END_ATOMIC;
	
	loop_until_bit_is_set(SPSR,SPIF);
	returnvar = SPDR;
	return returnvar;
}

void spi_v_write_block( const uint8_t *data, uint16_t length ){
	
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
// note: reading 0 bytes will still clock 1 byte in.
void spi_v_read_block( uint8_t *data, uint16_t length ){
	
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






