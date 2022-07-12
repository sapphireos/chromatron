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
 

#include <string.h>

#include "cpu.h"

#include "system.h"
#include "spi.h"

#include "flash_fs_partitions.h"
#include "flash25.h"

#ifdef __SIM__
    #define CHIP_ENABLE()
    #define CHIP_DISABLE()

    #define WRITE_PROTECT()
    #define WRITE_UNPROTECT()

    #define AAI_STATUS()        ( 1 )
    
    static uint8_t array[FLASH_FS_N_ERASE_BLOCKS * FLASH_FS_ERASE_BLOCK_SIZE];
    
#else
    #define CHIP_ENABLE() 		FLASH_CS_PORT &= ~_BV( FLASH_CS_PIN )
    #define CHIP_DISABLE() 		FLASH_CS_PORT |= _BV( FLASH_CS_PIN )

    #define WRITE_PROTECT() 	FLASH_WP_PORT &= ~_BV( FLASH_WP_PIN )
    #define WRITE_UNPROTECT() 	FLASH_WP_PORT |= _BV( FLASH_WP_PIN )

    #define AAI_STATUS()        ( SPI_PIN & _BV(SPI_MISO) )
#endif

void flash25_v_init( void ){
	
    #ifndef __SIM__
	// set CS to output
	FLASH_CS_DDR |= _BV(FLASH_CS_PIN);
    
    // set WP to output
    FLASH_WP_DDR |= _BV(FLASH_WP_PIN);
	
    
	// enable the write protect
	WRITE_PROTECT();
	
	// set the CS line to inactive
	CHIP_DISABLE();
    
    // send write disable
    flash25_v_write_disable();
    
    // disable busy status output on SO line
    CHIP_ENABLE();
    spi_u8_send( FLASH_CMD_DBUSY );
    CHIP_DISABLE();
    
	// enable writes
	flash25_v_write_enable();
	
	// clear block protection bits
	flash25_v_write_status( 0x00 );
    #endif
}

uint8_t flash25_u8_read_status( void ){
	
    #ifdef __SIM__
    return 0;
    #else
	uint8_t status;
	
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_READ_STATUS );
	status = spi_u8_send( 0 );
	
	CHIP_DISABLE();
	
	return status;
    #endif
}

bool flash25_b_busy( void ){
	
    #ifdef __SIM__
    return FALSE;
    #else
	return flash25_u8_read_status() & FLASH_STATUS_BUSY;
    #endif
}

void flash25_v_write_status( uint8_t status ){
    
    #ifndef __SIM__
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_ENABLE_WRITE_STATUS );
	
	CHIP_DISABLE();
	
	
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_WRITE_STATUS );
	spi_u8_send( status );
	
	CHIP_DISABLE();
    #endif
}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){
	
    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){
        
        return;
    }
    
    #ifdef __SIM__
    memcpy( ptr, &array[address], len );
    #else
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_READ );
	spi_u8_send( address >> 16 );
	spi_u8_send( address >> 8 );
	spi_u8_send( address );
	
    spi_v_read_block( ptr, len );
    
	CHIP_DISABLE();
    #endif
}

// read a single byte
uint8_t flash25_u8_read_byte( uint32_t address ){
	
	uint8_t byte;
	
	flash25_v_read( address, &byte, sizeof(byte) );
	
	return byte;
}

void flash25_v_write_enable( void ){
	
    #ifndef __SIM__
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_WRITE_ENABLE );
	
	CHIP_DISABLE();
	
	WRITE_UNPROTECT();
    #endif
}

void flash25_v_write_disable( void ){
	
    #ifndef __SIM__
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_WRITE_DISABLE );
	
	CHIP_DISABLE();
	
	WRITE_PROTECT();
    #endif
}

// write a single byte to the device
void flash25_v_write_byte( uint32_t address, uint8_t byte ){
	
    #ifdef __SIM__
    array[address] = byte;
    #else
	// wait on busy bit in status register
	// note that if the flash chip malfunctions and never completes
	// an operation, this will cause the entire system to hang
	// until the watchdog kicks it.  this is acceptable,
	// since at that point we have a hardware failure anyway.
	while( ( flash25_u8_read_status() & FLASH_STATUS_BUSY ) != 0 );
	
	flash25_v_write_enable();
	
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_WRITE_BYTE );
	spi_u8_send( address >> 16 );
	spi_u8_send( address >> 8 );
	spi_u8_send( address );
	
	spi_u8_send( byte );
	
	CHIP_DISABLE();
    #endif
}

// write an array of data
// this function will set the write enable
// this function will NOT check for a valid address, reaching the
// end of the array, etc.
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len ){
    
    // this could probably be an assert, since a write of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){
        
        return;
    }
    
    #ifdef __SIM__
    memcpy( &array[address], ptr, len );
    #else
    // check if odd address
    if( ( address & 1 ) != 0 ){
        
        flash25_v_write_byte( address, *(uint8_t *)ptr );
        
        address++;
        ptr++;
        len--;
    }
    
    if( len > 1 ){
        
        // busy wait
        while( ( flash25_u8_read_status() & FLASH_STATUS_BUSY ) != 0 );
        
        // enable writes
        flash25_v_write_enable();
        
        // enable busy status output on SO line
        CHIP_ENABLE();
        spi_u8_send( FLASH_CMD_EBUSY );
        CHIP_DISABLE();
        
        // use autoincrement write command
        CHIP_ENABLE();
        
        spi_u8_send( FLASH_CMD_AAI_WRITE );
        spi_u8_send( address >> 16 );
        spi_u8_send( address >> 8 );
        spi_u8_send( address );
        
        // write two bytes
        spi_u8_send( *(uint8_t *)ptr  );
        spi_u8_send( *(uint8_t *)(ptr + 1) );
        
        CHIP_DISABLE(); // this will begin the program cycle
        
        ptr += 2;
        len -= 2;
        address += 2;
        
        while( len > 1 ){
            
            // poll status output
            CHIP_ENABLE();
            _delay_us(FLASH_WAIT_DELAY_US);
            while( AAI_STATUS() == 0 );
            CHIP_DISABLE();
            
            CHIP_ENABLE();
            
            // send command
            spi_u8_send( FLASH_CMD_AAI_WRITE );
            
            // write two bytes
            spi_u8_send( *(uint8_t *)ptr  );
            spi_u8_send( *(uint8_t *)(ptr + 1) );
            
            CHIP_DISABLE(); // this will begin the program cycle
            
            ptr += 2;
            len -= 2;
            address += 2;
        }
        
        // poll status output
        CHIP_ENABLE();
        _delay_us(FLASH_WAIT_DELAY_US);
        while( AAI_STATUS() == 0 );
        CHIP_DISABLE();

        // send write disable to end command
        flash25_v_write_disable();
        
        // disable busy status output on SO line
        CHIP_ENABLE();
        spi_u8_send( FLASH_CMD_DBUSY );
        CHIP_DISABLE();
    }
    
    // check if there is data left
    // this also handles the case where there is only one byte to be written
    if( len == 1 ){
        
        flash25_v_write_byte( address, *(uint8_t *)ptr );
    }
    #endif
}

// erase a 4 KB block
// note that this command will wait on the busy bit to be
// clear before issuing the instruction to the device.
// since erases may take a long time, it would be best to
// check the busy bit before calling this function.
void flash25_v_erase_4k( uint32_t address ){
	
    #ifdef __SIM__
    memset( &array[address], 0xff, 4096 );
    #else
	while( ( flash25_u8_read_status() & FLASH_STATUS_BUSY ) != 0 );
	
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_ERASE_BLOCK_4K );
	spi_u8_send( address >> 16 );
	spi_u8_send( address >> 8 );
	spi_u8_send( address );
	
	CHIP_DISABLE();
    #endif
}

// erase the entire array
void flash25_v_erase_chip( void ){
	
    #ifdef __SIM__
    memset( array, 0xff, sizeof(array) );
    #else
	while( ( flash25_u8_read_status() & FLASH_STATUS_BUSY ) != 0 );
	
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_CHIP_ERASE );
	
	CHIP_DISABLE();
    #endif
}

void flash25_v_read_device_info( flash25_device_info_t *info ){
	
    #ifdef __SIM__
    
    info->mfg_id = FLASH_MFG_SST;
	info->dev_id_1 = FLASH_DEV_ID1_SST25;
	info->dev_id_2 = 0;
	
    #else
	CHIP_ENABLE();
	
	spi_u8_send( FLASH_CMD_READ_ID );
	
	info->mfg_id = spi_u8_send( 0 );
	info->dev_id_1 = spi_u8_send( 0 );
	info->dev_id_2 = spi_u8_send( 0 );
	
	CHIP_DISABLE();
	#endif
}

uint8_t flash25_u8_read_mfg_id( void ){

	flash25_device_info_t info;	
	
	flash25_v_read_device_info( &info );
	
	return info.mfg_id;
}

// returns capacity in bytes
uint32_t flash25_u32_capacity( void ){
	
	flash25_device_info_t info;	
	
	flash25_v_read_device_info( &info );
	
	uint32_t capacity = 0;
	
	switch( info.mfg_id ){
		
		case FLASH_MFG_ATMEL:
			
			if( info.dev_id_1 == 0x47 ){
				
				capacity = 1048576;
			}
			
			break;
		
		case FLASH_MFG_SST:
			
			if( info.dev_id_1 == 0x25 ){
				
				if( info.dev_id_2 == 0x8D ){
					
					capacity = 524288;
				}
			}
			
			break;
		
		default:
			break;
	}
	
	return capacity;
}

