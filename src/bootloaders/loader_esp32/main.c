/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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
#include "io.h"
#include "flash_fs.h"

#include "bootloader_config.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "esp32/rom/rtc.h"
#include "soc/rtc_wdt.h"

// bootloader shared memory
extern boot_data_t BOOTDATA boot_data;

void main( void ){

    trace_printf("Welcome to Sapphire ESP32 Bootloader\n");

    cpu_v_init();

    // init CRC
    crc_v_init();

    rtc_wdt_disable();

    // check reset source
    uint8_t reset_source = cpu_u8_get_reset_source();

    // check for power on reset (or brown out, or external reset...)
    if( reset_source != 0 ){

        boot_data.loader_command = LDR_CMD_NONE; // init loader command
        boot_data.reboots = 0; // initialize reboot counter
        boot_data.boot_mode = BOOT_MODE_REBOOT; // signal so app doesn't go into safe mode
    }

    trace_printf("Reset source: %d\n", reset_source );

    boot_data.loader_version_major = LDR_VERSION_MAJOR;
    boot_data.loader_version_minor = LDR_VERSION_MINOR;

    boot_data.loader_status = LDR_STATUS_NORMAL;

    // initialize external flash
    flash25_v_init();

    trace_printf("Board type: %d\n", ffs_u8_read_board_type() );

    io_v_init();

    ldr_v_set_yellow_led();
    
goto start;

    if( FW_START_OFFSET == 0xffffffff ){

        trace_printf("Internal image fail\n");

        goto fatal_error;
    }

    // check integrity of internal firmware
    uint16_t internal_crc = ldr_u16_get_internal_crc();
    trace_printf("Internal crc: 0x%04x\n", internal_crc);

    // check if internal partition is damaged
    if( internal_crc != 0 ){

        // check partition
        uint16_t partition_crc = ldr_u16_get_partition_crc();
        trace_printf("Partition crc: 0x%04x\n", partition_crc);

        if( partition_crc != 0 ){

            // internal and partition are bad.

            goto fatal_error;
        }

        trace_printf("Recovering internal FW...\n");

        // try to recover several times - we really don't want to
        // fail here if we can avoid it
        for( uint8_t i = 0; i < 5; i++ ){

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

    // check firmware status
    if( internal_crc != 0 ){

        // internal code is damaged (and could not be recovered)
        // we cannot recover from this
        goto fatal_error;
    }

    trace_printf("Loader cmd: %04x\n", boot_data.loader_command);

    // check loader command
    if( boot_data.loader_command == LDR_CMD_LOAD_FW ){

        trace_printf("Load new FW requested\n");

        // check partition
        uint16_t partition_crc = ldr_u16_get_partition_crc();
        trace_printf("Partition crc: 0x%04x\n", partition_crc);

        // check that partition CRC is ok
        if( partition_crc == 0 ){

            trace_printf("Copying FW...\n");

            // partition CRC is ok
            // copy partition to the internal flash
            ldr_v_copy_partition_to_internal();

            // recompute internal CRC
            internal_crc = ldr_u16_get_internal_crc();

            trace_printf("Internal crc: 0x%04x\n", internal_crc);

            // set loader status
            boot_data.loader_status = LDR_STATUS_NEW_FW;
            boot_data.boot_mode = BOOT_MODE_REBOOT;

            // check internal CRC
            if( internal_crc != 0 ){

                goto fatal_error;
            }
        }
        // partition CRC is bad
        else{
            
            trace_printf("Bad FW CRC\n");

            boot_data.loader_status = LDR_STATUS_PARTITION_CRC_BAD;
        }
    }

start:
    // clear loader command
    boot_data.loader_command = LDR_CMD_NONE;

    // run application
    ldr_v_clear_yellow_led();

    // ldr_run_app();
    trace_printf("Starting main app...\n");
    return;


fatal_error:

    trace_printf("FATAL ERROR\n");

    ldr_v_clear_yellow_led();
    ldr_v_set_red_led();

    while(1);
}



/*
 * Selects a boot partition.
 * The conditions for switching to another firmware are checked.
 */
static int selected_boot_partition(const bootloader_state_t *bs)
{
    int boot_index = bootloader_utility_get_selected_boot_partition(bs);
    if (boot_index == INVALID_INDEX) {
        return boot_index; // Unrecoverable failure (not due to corrupt ota data or bad partition contents)
    }
    if (rtc_get_reset_reason(0) != DEEPSLEEP_RESET) {
        // Factory firmware.
#ifdef CONFIG_BOOTLOADER_FACTORY_RESET
        if (bootloader_common_check_long_hold_gpio(CONFIG_BOOTLOADER_NUM_PIN_FACTORY_RESET, CONFIG_BOOTLOADER_HOLD_TIME_GPIO) == 1) {
            ESP_LOGI(TAG, "Detect a condition of the factory reset\n");
            bool ota_data_erase = false;
#ifdef CONFIG_BOOTLOADER_OTA_DATA_ERASE
            ota_data_erase = true;
#endif
            const char *list_erase = CONFIG_BOOTLOADER_DATA_FACTORY_RESET;
            ESP_LOGI(TAG, "Data partitions to erase: %s", list_erase);
            if (bootloader_common_erase_part_type_data(list_erase, ota_data_erase) == false) {
                trace_printf("Not all partitions were erased\n");
            }
            return bootloader_utility_get_selected_boot_partition(bs);
        }
#endif
       // TEST firmware.
#ifdef CONFIG_BOOTLOADER_APP_TEST
        if (bootloader_common_check_long_hold_gpio(CONFIG_BOOTLOADER_NUM_PIN_APP_TEST, CONFIG_BOOTLOADER_HOLD_TIME_GPIO) == 1) {
            ESP_LOGI(TAG, "Detect a boot condition of the test firmware\n");
            if (bs->test.offset != 0) {
                boot_index = TEST_APP_INDEX;
                return boot_index;
            } else {
                trace_printf("Test firmware is not found in partition table\n");
                return INVALID_INDEX;
            }
        }
#endif
        // Customer implementation.
        // if (gpio_pin_1 == true && ...){
        //     boot_index = required_boot_partition;
        // } ...
    }
    return boot_index;
}


// Select the number of boot partition
static int select_partition_number (bootloader_state_t *bs)
{
    // 1. Load partition table
    if (!bootloader_utility_load_partition_table(bs)) {
        trace_printf("load partition table error!\n");
        return INVALID_INDEX;
    }

    // 2. Select the number of boot partition
    return selected_boot_partition(bs);
}



void __attribute__((noreturn)) call_start_cpu0()
{
    // 1. Hardware initialization
    if (bootloader_init() != ESP_OK) {
        bootloader_reset();
    }

    // run Sapphire bootloader
    main();
    // after we exit, we let the ESP bootloader take over and load the app

    // 2. Select the number of boot partition
    bootloader_state_t bs = { 0 };
    int boot_index = select_partition_number(&bs);
    if (boot_index == INVALID_INDEX) {
        bootloader_reset();
    }

    // 3. Load the app image for booting
    bootloader_utility_load_boot_image(&bs, boot_index);


    while(1);
}

