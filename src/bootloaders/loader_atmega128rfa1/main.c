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
 

/*

Sapphire Shadow Boot Loader


Boot sequence:

Init status LEDs
Set clock to 16 MHz
Read mode from EEPROM

Sapphire mode:
    Check button, if down:
        Run serial processor with 5 second timeout
    
    Check loader mode
        If load firmware:
            Check external CRC
            Copy to internal

        Else:
            Check internal CRC
            If bad, copy from external
    
    Run app

Generic mode:
    Check button, if down:
        Run serial processor, no timeout
    
    Else:
        Run app firmware (no CRC checks)


*/


#include "cpu.h"

#include "eeprom.h"
#include "spi.h"
#include "crc.h"
#include "bootcore.h"
#include "boot_data.h"
#include "serial.h"
#include "loader.h"
#include "flash25.h"
#include "button.h"


static uint8_t mode;
#define MODE_SAPPHIRE   0
#define MODE_GENERIC    1

char sw_id[7];

// bootloader shared memory
volatile boot_data_t BOOTDATA boot_data;



void main( void ){

restart:

	cli();
    
    ldr_v_set_yellow_led();
	
	// system clock is at 16 MHz
	ldr_v_set_clock_prescaler( CLK_DIV_1 );
	
	// disable watchdog timer
	wdt_reset();
	MCUSR &= ~( 1 << WDRF ); // ensure watchdog reset is clear
    WDTCSR |= ( 1 << WDCE ) | ( 1 << WDE );
	WDTCSR = 0x00;

	//WDTCSR |= (1<<WDCE) | (1<<WDE); // enable watchdog change
	//WDTCSR = (1<<WDE) | (1<<WDP2) | (0<<WDP1) | (0<<WDP0); // set watchdog timer to 0.25 seconds

    // check reset source
    uint8_t reset_source = MCUSR;
    
    // check for power on reset
    if( reset_source & ( 1 << PORF ) ){
        
        boot_data.loader_command = LDR_CMD_NONE; // init loader command
    }

    boot_data.loader_version_major = LDR_VERSION_MAJOR;
    boot_data.loader_version_minor = LDR_VERSION_MINOR;
    
    boot_data.loader_status = LDR_STATUS_NORMAL;

    // // check if button is held down, or if the serial boot command was set
    // if( ( button_b_is_pressed() ) || ( boot_data.loader_command == LDR_CMD_SERIAL_BOOT ) ){
        
    //     serial_v_loop( TRUE );
    // }
    
    // initialize spi
    spi_v_init( 0, 0, 0 );


    // initialize external flash
    flash25_v_init();

    // check integrity of internal firmware
    uint16_t internal_crc = ldr_u16_get_internal_crc();

    // reset watchdog timer
    wdt_reset();
    
    // check if internal partition is damaged
    if( internal_crc != 0 ){
        
        // check external partition crc
        if( ldr_u16_get_partition_crc() == 0 ){
            
            // partition CRC is ok
            // copy partition to the internal flash
            ldr_v_copy_partition_to_internal();
            
            // set loader status
            boot_data.loader_status = LDR_STATUS_RECOVERED_FW;

            // recompute internal crc
            internal_crc = ldr_u16_get_internal_crc();
        }
    }

    // check firmware status
    if( internal_crc != 0 ){
        
        // internal code is damaged (and could not be recovered)
        // we cannot recover from this
        goto fatal_error; 
    }
    
    // check loader command
    if( boot_data.loader_command == LDR_CMD_LOAD_FW ){
        
        // check that partition CRC is ok
        if( ldr_u16_get_partition_crc() == 0 ){
            
            // partition CRC is ok
            // copy partition to the internal flash
            ldr_v_copy_partition_to_internal();
            
            // recompute internal CRC
            internal_crc = ldr_u16_get_internal_crc();
            
            // set loader status
            boot_data.loader_status = LDR_STATUS_NEW_FW;

            // check internal CRC
            if( internal_crc != 0 ){

                goto fatal_error; 
            }
        }
    }

    // clear loader command
    boot_data.loader_command = LDR_CMD_NONE;

    ldr_v_set_green_led();
    
    // run application
    ldr_run_app();

    
fatal_error:
    
    ldr_v_set_red_led();

    // serial_v_loop( FALSE );
    
    // if the serial processor exits, we restart the loader
    // goto restart;
    while(1);
}

