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



#include "system.h"
#include "target.h"

#include "boot_data.h"
#include "crc.h"
#include "eeprom.h"
#include "flash25.h"
#include "hal_flash25.h"
#include "flash_fs_partitions.h"
#include "spi.h"
#include "watchdog.h"
#include "hal_nvm.h"
#include "hal_io.h"

#include "loader.h"

#include "bootloader_flash.h"
#include "bootloader_flash_priv.h"
#include "esp_image_format.h"
#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "esp32/rom/cache.h"

#include <string.h>


// esp_err_t spi_flash_read( size_t src, void *dest, size_t size ){

//     return bootloader_flash_read( src, dest, size, false );
// }

void ldr_v_erase_app( void ){

	uint32_t offset = FW_SPI_START_OFFSET / 4096;

	trace_printf("Erasing firmware\n");

	for( uint32_t i = 0; i < FLASH_FS_FIRMWARE_0_N_BLOCKS; i++ ){

        bootloader_flash_erase_sector( i + offset );
	}
	// trace_printf("Done\n");
}

void ldr_v_copy_partition_to_internal( void ){

    ldr_v_erase_app();

    uint32_t length = ldr_u32_read_partition_length();
	
	trace_printf("Writing firmware\n");

    uint8_t data[256] __attribute__((aligned(4)));

	for( uint32_t i = 0; i < length; i += sizeof(data) ){

		// load page data
		ldr_v_read_partition_data( i, (void *)&data, sizeof(data) );

		bootloader_flash_write( i + FW_SPI_START_OFFSET, data, sizeof(data), FALSE );

		// reset watchdog timer
		wdg_v_reset();
	}

	// trace_printf("Done\n");
}

void ldr_run_app( void ){

}


// read data from an external partition
void ldr_v_read_partition_data( uint32_t offset, uint8_t *dest, uint16_t length ){

	flash25_v_read( offset + FLASH_FS_FIRMWARE_0_PARTITION_START, dest, length );
}

// return length of an external partition (actual length of file, not partition size)
uint32_t ldr_u32_read_partition_length( void ){

	uint32_t partition_length;
    
    uint32_t fw_start_offset = hal_flash25_u32_get_partition_start();

    if( fw_start_offset == 0xffffffff ){

        return 0;
    }

    // trace_printf("Partition FW_START_OFFSET 0x%0x\n", fw_start_offset);

    uint32_t address = FLASH_FS_FIRMWARE_0_PARTITION_START + fw_start_offset;

	flash25_v_read( address, &partition_length, sizeof(partition_length) );

    // trace_printf("ldr_u32_read_partition_length 0x%0x %d\n", address, partition_length);
	
    partition_length += sizeof(uint16_t);

    // check max length
    if( partition_length > FW_MAX_SIZE ){

        return 0;
    }

	return partition_length;
}

// return the application length
uint32_t ldr_u32_read_internal_length( void ){

	uint32_t internal_length = 0;

	uint32_t addr = FW_START_OFFSET;

    if( addr == 0xffffffff ){

        return 0;
    }

	spi_flash_read( addr, &internal_length, sizeof(internal_length) );

	internal_length += sizeof(uint16_t);

	return internal_length;
}

uint16_t ldr_u16_get_internal_crc( void ){

	uint16_t crc = crc_u16_start();

	uint32_t length = ldr_u32_read_internal_length();

	trace_printf("Internal len: %u\n", length);

    if( length < 4 ){

        // obviously bad length
        return 0xffff;
    }

    uint32_t remaining = length;

    uint8_t buf[256] __attribute__((aligned(4)));

	for( uint32_t i = 0; i < length; i += sizeof(buf) ){

        uint16_t copy_len = sizeof(buf);

        if( copy_len > remaining ){

            copy_len = remaining;
        }

        remaining -= copy_len;

		spi_flash_read( i + FW_SPI_START_OFFSET, buf, copy_len );

        for( uint32_t j = 0; j < copy_len; j++ ){

            crc = crc_u16_byte( crc, buf[j] );    
        }
		
		// reset watchdog timer
		wdg_v_reset();
	}

	return crc_u16_finish( crc );
}

// compute CRC of external partition
uint16_t ldr_u16_get_partition_crc( void ){

	uint16_t crc = crc_u16_start();

	uint32_t length = ldr_u32_read_partition_length();

	trace_printf("Partition len: %u\n", length);

    uint32_t address = FLASH_FS_FIRMWARE_0_PARTITION_START;

	while( length > 0 ){

        uint8_t buf[256] __attribute__((aligned(4)));
        uint32_t copy_len = sizeof(buf);

        if( copy_len > length ){

            copy_len = length;
        }

        flash25_v_read( address, buf, copy_len );

        // trace_printf("0x%08x\n", *(uint32_t*)buf);

        address += copy_len;
        length -= copy_len;

        crc = crc_u16_partial_block( crc, buf, copy_len );

		// reset watchdog timer
		wdg_v_reset();
	}

	return crc_u16_finish( crc );
}
