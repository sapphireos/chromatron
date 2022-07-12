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

#include "system.h"
#include "target.h"

#include "eeprom.h"

#ifdef __SIM__
static uint8_t array[EE_ARRAY_SIZE];
#endif

// write a byte to eeprom and wait for the completion of the write
void ee_v_write_byte_blocking( uint16_t address, uint8_t data ){
	
    #ifdef __SIM__
    
    array[address] = data;
    
    #else
    
	ATOMIC; // must be atomic, an interrupt here will cause the write to fail
	
	EEAR = address; // set address
	EEDR = data; // set byte to be written
	
	// pre-erase and write
	EECR &= ~_BV( EEPM0 );
	EECR &= _BV( EEPM1 );
	
	EECR |= _BV( EEMPE ); // set master programming enable
	EECR |= _BV( EEPE ); // set programming enable
	
	END_ATOMIC;
	
	while( ( EECR & _BV( EEPE ) ) != 0 );
    
    #endif
}

void ee_v_write_block( uint16_t address, uint8_t *data, uint16_t len ){
    
    while( len > 0 ){
        
        ee_v_write_byte_blocking( address, *data );
        
        data++;
        len--;
        address++;
        
        wdt_reset();
    }
}

uint8_t ee_u8_read_byte( uint16_t address ){
	
    #ifdef __SIM__
    
    return array[address];
    
    #else
    
	uint8_t sreg = SREG;
	
	cli();
	
	EEAR = address; // set address
	
	EECR |= _BV( EERE ); // set read enable
	
	SREG = sreg;
	
	return EEDR; // return byte
    
    #endif
}

void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t length ){
	
	for( uint16_t i = 0; i < length; i++ ){
		
		*data = ee_u8_read_byte( address );
		
		data++;
		address++;
	}
}
