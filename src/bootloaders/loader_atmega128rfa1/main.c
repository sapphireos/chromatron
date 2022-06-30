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
    
    // set status LED pins to outputs
    LDR_LED_GREEN_DDR |= _BV(LDR_LED_GREEN_PIN);
    LDR_LED_YELLOW_DDR |= _BV(LDR_LED_YELLOW_PIN);
    LDR_LED_RED_DDR |= _BV(LDR_LED_RED_PIN);
	
	// system clock is at 16 MHz
	ldr_v_set_clock_prescaler( CLK_DIV_1 );
	
	// disable watchdog timer
	wdt_reset();
	MCUSR &= ~( 1 << WDRF ); // ensure watchdog reset is clear
    WDTCSR |= ( 1 << WDCE ) | ( 1 << WDE );
	WDTCSR = 0x00;

	//WDTCSR |= (1<<WDCE) | (1<<WDE); // enable watchdog change
	//WDTCSR = (1<<WDE) | (1<<WDP2) | (0<<WDP1) | (0<<WDP0); // set watchdog timer to 0.25 seconds
	
    // read mode string from EEPROM
    char mode_string[9];
    ee_v_read_block( 0, (uint8_t *)mode_string, sizeof(mode_string) - 1 );
    mode_string[8] = 0;
    
    mode = MODE_GENERIC;
    sw_id[0] = 'S';
    sw_id[1] = 'O';
    sw_id[2] = 'S';
    sw_id[3] = '_';
    sw_id[4] = 'L';
    sw_id[5] = 'D';
    sw_id[6] = '0';

    // check mode
    if( strcmp( mode_string, "Sapphire" ) == 0 ){
        
        mode = MODE_SAPPHIRE;

        sw_id[0] = 'S';
        sw_id[1] = 'O';
        sw_id[2] = 'S';
        sw_id[3] = '_';
        sw_id[4] = 'L';
        sw_id[5] = 'D';
        sw_id[6] = '1';
    }
    
    ldr_v_set_yellow_led();

    if( mode == MODE_SAPPHIRE ){

        // check reset source
        uint8_t reset_source = MCUSR;
        
        // check for power on reset
        if( reset_source & ( 1 << PORF ) ){
            
            boot_data.loader_command = LDR_CMD_NONE; // init loader command
        }

        boot_data.loader_version_major = LDR_VERSION_MAJOR;
        boot_data.loader_version_minor = LDR_VERSION_MINOR;
        
        boot_data.loader_status = LDR_STATUS_NORMAL;

        // check if button is held down, or if the serial boot command was set
        if( ( button_b_is_pressed() ) || ( boot_data.loader_command == LDR_CMD_SERIAL_BOOT ) ){
            
            serial_v_loop( TRUE );

            // check if we erased the eeprom to change the boot mode,
            // if so, restart the loader.  This is because if we restart into the app,
            // the OS will put us back in sapphire mode.
            if( strcmp( mode_string, "Sapphire" ) != 0 ){

                goto restart;
            }
            
            // If we're still in Sapphire mode, then after the serial loop times out,
            // run the normal loader routines before starting the OS.
        }
        
        // initialize spi
        spi_v_init();
        
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
        
        // run application
        ldr_run_app();

        
fatal_error:
        
        ldr_v_set_red_led();

        serial_v_loop( FALSE );
        
        // if the serial processor exits, we restart the loader
        goto restart;
    }
    else{ // generic mode
        
        // check if button is pressed, or if firmware is not present
        if( ( button_b_is_pressed() ) || ( pgm_read_byte( 0 ) == 0xff ) ){
            
            serial_v_loop( FALSE );
        }
            
        ldr_v_clear_yellow_led();
            
        ldr_run_app();
    }
}

