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
#include "boot_data.h"
#include "loader.h"
#include "flash25.h"
#include "hal_cpu.h"
#include "crc.h"
#include "hal_status_led.h"
#include "watchdog.h"

// bootloader shared memory
extern boot_data_t BOOTDATA boot_data;

#ifdef ENABLE_COPROCESSOR
#error "ESP8266 bootloader not compatible with ENABLE_COPROCESSOR!"
#endif


#define INIT_DATA_ADDR 0x003fc000
static uint8_t esp_init_data[] = {
    #include "esp_init_data_default_v08.txt"
};

void main( void ){

    trace_printf("Welcome to Sapphire\n");
    trace_printf("ESP8266 Bootloader\n");

    // hal_cpu_v_load_bootdata();

    // init CRC
    crc_v_init();

    ldr_v_set_yellow_led();

    // check reset source
    uint8_t reset_source = cpu_u8_get_reset_source();

    trace_printf("Reset source: %d\n", reset_source);

    boot_data.loader_version_major = LDR_VERSION_MAJOR;
    boot_data.loader_version_minor = LDR_VERSION_MINOR;

    boot_data.loader_status = LDR_STATUS_NORMAL;

    // initialize external flash
    flash25_v_init();

    // check init data
    uint8_t init = 0xff;
    SPIRead( INIT_DATA_ADDR, (void *)&init, 1 );
    if( init == 0xff ){

        // need to write init data
        SPIEraseSector( INIT_DATA_ADDR / 4096 );
        SPIWrite( INIT_DATA_ADDR, esp_init_data, sizeof(esp_init_data) );   
    }

    

    // // check integrity of internal firmware
    // uint16_t internal_crc = ldr_u16_get_internal_crc();
    // trace_printf("Internal crc: 0x%04x\n", internal_crc);

    // // check if internal partition is damaged
    // if( internal_crc != 0 ){

    //     // check partition
    //     uint16_t partition_crc = ldr_u16_get_partition_crc();
    //     trace_printf("Partition crc: 0x%04x\n", partition_crc);

    //     if( partition_crc != 0 ){

    //         // internal and partition are bad.

    //         goto fatal_error;
    //     }

    //     trace_printf("Recovering internal FW...\n");

    //     // try to recover several times - we really don't want to
    //     // fail here if we can avoid it
    //     for( uint8_t i = 0; i < 5; i++ ){

    //         // copy partition to the internal flash
    //         ldr_v_copy_partition_to_internal();

    //         // set loader status
    //         boot_data.loader_status = LDR_STATUS_RECOVERED_FW;

    //         // recompute internal crc
    //         internal_crc = ldr_u16_get_internal_crc();

    //         if( internal_crc == 0 ){

    //             break;
    //         }
    //     }
    // }

    // // check firmware status
    // if( internal_crc != 0 ){

    //     // internal code is damaged (and could not be recovered)
    //     // we cannot recover from this
    //     goto fatal_error;
    // }

    // trace_printf("Loader cmd: %04x\n", boot_data.loader_command);

    // // check loader command
    // if( boot_data.loader_command == LDR_CMD_LOAD_FW ){

    //     trace_printf("Load new FW requested\n");

    //     // check partition
    //     uint16_t partition_crc = ldr_u16_get_partition_crc();
    //     trace_printf("Partition crc: 0x%04x\n", partition_crc);

    //     // check that partition CRC is ok
    //     if( partition_crc == 0 ){

    //         trace_printf("Copying FW...\n");

    //         // partition CRC is ok
    //         // copy partition to the internal flash
    //         ldr_v_copy_partition_to_internal();

    //         // recompute internal CRC
    //         internal_crc = ldr_u16_get_internal_crc();

    //         trace_printf("Internal crc: 0x%04x\n", internal_crc);

    //         // set loader status
    //         boot_data.loader_status = LDR_STATUS_NEW_FW;
    //         boot_data.boot_mode = BOOT_MODE_REBOOT;

    //         // check internal CRC
    //         if( internal_crc != 0 ){

    //             goto fatal_error;
    //         }
    //     }
    //     // partition CRC is bad
    //     else{
            
    //         trace_printf("Bad FW CRC\n");

    //         boot_data.loader_status = LDR_STATUS_PARTITION_CRC_BAD;
    //     }
    // }

    // if( flash25_u8_read_byte( BOOTLOADER_INFO_BLOCK ) != 0xff ){

    //     // erase stored bootdata
    //     flash25_v_erase_4k( BOOTLOADER_INFO_BLOCK );
    // }

    // // clear loader command
    // boot_data.loader_command = LDR_CMD_NONE;

    // run application
    ldr_v_clear_yellow_led();

    ldr_run_app();


// fatal_error:

//     trace_printf("FATAL ERROR\n");

//     ldr_v_clear_yellow_led();
//     ldr_v_set_red_led();

//     while(1);
}

