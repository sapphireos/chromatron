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
#include "bootcore.h"
#include "boot_data.h"
#include "serial.h"
#include "loader.h"
#include "flash25.h"
#include "button.h"
#include "hal_cpu.h"
#include "hal_status_led.h"
#include "watchdog.h"

// bootloader shared memory
extern boot_data_t BOOTDATA boot_data;

void main( void ){

    cpu_v_init();

    trace_printf("Welcome to Sapphire\n");


    LL_GPIO_InitTypeDef GPIO_InitStruct;

    LL_GPIO_SetOutputPin(LED_RED_PORT,      LED_RED_PIN);
    LL_GPIO_SetOutputPin(LED_GREEN_PORT,    LED_GREEN_PIN);
    LL_GPIO_SetOutputPin(LED_BLUE_PORT,     LED_BLUE_PIN);

    GPIO_InitStruct.Pin         = LED_RED_PIN;
    GPIO_InitStruct.Mode        = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed       = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType  = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull        = LL_GPIO_PULL_NO;
    LL_GPIO_Init(LED_RED_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = LED_GREEN_PIN;
    GPIO_InitStruct.Mode        = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed       = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType  = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull        = LL_GPIO_PULL_NO;
    LL_GPIO_Init(LED_GREEN_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = LED_BLUE_PIN;
    GPIO_InitStruct.Mode        = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed       = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType  = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull        = LL_GPIO_PULL_NO;
    LL_GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);


    trace_printf("CPU Clock: %u\n", cpu_u32_get_clock_speed());



while(1){

    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1);
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_2);
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_3); 
    HAL_Delay(500);
    
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_1);
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_2);
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_3); 
    HAL_Delay(500);

    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_1);
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_2);
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_3); 
    HAL_Delay(500);
}


restart:

	DISABLE_INTERRUPTS;

    cpu_v_init();

	// disable watchdog timer
    wdg_v_disable();

    // init boot module
    boot_v_init();

    // init button (if present)
    button_v_init();

    ldr_v_set_yellow_led();

    // check reset source
    uint8_t reset_source = cpu_u8_get_reset_source();

    // check for power on reset
    if( reset_source & RESET_SOURCE_POWER_ON ){

        boot_data.loader_command = LDR_CMD_NONE; // init loader command
    }

    boot_data.loader_version_major = LDR_VERSION_MAJOR;
    boot_data.loader_version_minor = LDR_VERSION_MINOR;

    boot_data.loader_status = LDR_STATUS_NORMAL;

    // check if button is held down, or if the serial boot command was set
    if( ( button_b_is_pressed() ) || ( boot_data.loader_command == LDR_CMD_SERIAL_BOOT ) ){

        ldr_v_clear_yellow_led();
        ldr_v_set_green_led();
        // _delay_ms( 250 );
        ldr_v_clear_green_led();
        ldr_v_set_yellow_led();

        serial_v_loop( TRUE );
    }

    // initialize spi
    spi_v_init();

    // initialize external flash
    flash25_v_init();

    // check integrity of internal firmware
    uint16_t internal_crc = ldr_u16_get_internal_crc();

    // check if internal partition is damaged
    if( internal_crc != 0 ){

        // try to recover several times - we really don't want to
        // fail here if we can avoid it
        for( uint8_t i = 0; i < 5; i++ ){

            // check external partition crc
            if( ldr_u16_get_partition_crc() == 0 ){

                // partition CRC is ok
                // copy partition to the internal flash
                ldr_v_copy_partition_to_internal();

                // set loader status
                boot_data.loader_status = LDR_STATUS_RECOVERED_FW;

                // recompute internal crc
                internal_crc = ldr_u16_get_internal_crc();

                if( internal_crc == 0 ){

                    break;
                }
            }
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
        // partition CRC is bad
        else{

            boot_data.loader_status = LDR_STATUS_PARTITION_CRC_BAD;
        }
    }

    // clear loader command
    boot_data.loader_command = LDR_CMD_NONE;

    // run application
    ldr_run_app();


fatal_error:

    ldr_v_clear_yellow_led();
    ldr_v_set_red_led();

    serial_v_loop( FALSE );

    // if the serial processor exits, we restart the loader
    goto restart;    
}
