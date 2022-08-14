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

#include "system.h"
#include "timers.h"
#include "flash_fs_partitions.h"
#include "flash25.h"
#include "hal_flash25.h"
#include "hal_io.h"
#include "keyvalue.h"

#include "esp32/rom/spi_flash.h"
#include "esp_spi_flash.h"
#include "esp_image_format.h"

#ifdef ENABLE_FFS

static uint32_t flash_id;
static uint8_t flash_erase_time;

KV_SECTION_META kv_meta_t flash_id_kv[] = {
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_id, 0,          "flash_id" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &flash_erase_time, 0,  "flash_erase_time" },
};


// FW_SPI_START_OFFSET offsets from the end of the bootloader section.
#define START_ADDRESS ( FLASH_FS_FIRMWARE_0_PARTITION_SIZE + FW_SPI_START_OFFSET )

extern uint16_t block0_unlock;
extern uint16_t eeprom_erase_unlock;

static uint32_t max_address;

#define CACHE_ADDR_INVALID 0xffffffff

typedef union{
    uint32_t word;
    uint8_t bytes[4];
} cache_data_t;

// the ESP8266 can only access flash on a 32 bit alignment, so 
// we are doing a simple word cache.
// cache uses physical address, not logical.
static cache_data_t cache_data;
static uint32_t cache_address;
static bool cache_dirty;

static int spi_read( uint32_t address, uint32_t *ptr, uint32_t size ){

    address += START_ADDRESS;

    return spi_flash_read( address, ptr, size );
}

static int spi_write( uint32_t address, uint32_t *ptr, uint32_t size ){

    address += START_ADDRESS;

    #ifdef BOOTLOADER
    return 0;
    #else
    return spi_flash_write( address, ptr, size );
    #endif
}

static void flush_cache( void ){

    // cache is clean, nothing to do
    if( !cache_dirty ){

        return;
    }

    // commit to flash
    spi_write( cache_address, &cache_data.word, sizeof(cache_data) );

    // trace_printf("cache flush: 0x%08x = 0x%08x\r\n", cache_address, cache_data.word);
    
    // cache is now valid
    cache_dirty = FALSE;
}

static void load_cache( uint32_t address ){

    if( cache_dirty ){

        flush_cache();
    }

    spi_read( address, &cache_data.word, sizeof(cache_data) );

    cache_address = address;

    // trace_printf("cache load:  0x%08x = 0x%08x\r\n", cache_address, cache_data.word);
}

// flush cache and then set address to an invalid range.
// this will force a reload on the next read.
static void invalidate_cache( void ){

    flush_cache();
    cache_address = CACHE_ADDR_INVALID;
}


uint32_t hal_flash25_u32_get_partition_start( void ){

    esp_image_header_t header;
    uint32_t addr = FLASH_FS_FIRMWARE_0_PARTITION_START;
    spi_read( addr, (uint32_t *)&header, sizeof(header) );
    addr += sizeof(header);

    if( header.segment_count == 0xff ){

        trace_printf("PARTITION Image invalid!\n");        

        return 0xffffffff;
    }

    uint32_t start = 0;

    trace_printf("PARTITION Image info\nSegments: %d\n", header.segment_count);

    for( uint8_t i = 0; i < header.segment_count; i++ ){

        esp_image_segment_header_t seg_header;
        spi_read( addr, (uint32_t *)&seg_header, sizeof(seg_header) );

        trace_printf("Segment load: 0x%0x len: %u offset: 0x%0x\n", seg_header.load_addr, seg_header.data_len, addr - FLASH_FS_FIRMWARE_0_PARTITION_START);

        if( seg_header.load_addr == FW_LOAD_ADDR ){

            start = addr + sizeof(seg_header) - FLASH_FS_FIRMWARE_0_PARTITION_START;
        }

        addr += sizeof(seg_header);
        addr += seg_header.data_len;
    }

    return start;
}


void hal_flash25_v_init( void ){

    // read max address
    max_address = flash25_u32_read_capacity_from_info();

    if( max_address >= ( START_ADDRESS + ( 64 *1024 ) ) ){
        
        max_address -= START_ADDRESS;
        max_address -= ( 32 * 1024 ); // reserve space for SDK config area
    }
    else{

        trace_printf("Invalid flash size!\r\n");

        // invalid flash
        max_address = 0;
    }
    
    trace_printf("Flash capacity: %d\r\n", max_address);
    

    // enable writes
    flash25_v_write_enable();

    // clear block protection bits
    flash25_v_write_status( 0x00 );

    // disable writes
    flash25_v_write_disable();

    // init cache to invalid to force a load on the first read
    cache_address = CACHE_ADDR_INVALID;
}

uint8_t flash25_u8_read_status( void ){

    uint8_t status = 0;

    return status;
}

void flash25_v_write_status( uint8_t status ){


}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    ASSERT( address < max_address );

    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

    // fix odd address alignment
    while( ( address % 4 ) != 0 ){

        *(uint8_t *)ptr = flash25_u8_read_byte( address );

        address++;
        ptr++;
        len--;
    }

    uint32_t block_len = ( len / 4 ) * 4;

    // misaligned pointers will cause an exception!
    // if our alignment is off, we'll copy into an aligned buffer
    // and do the flash write from there.
    if( ( (uint32_t)ptr % 4 ) != 0 ){

        uint8_t buf[64] __attribute__((aligned(4)));

        while( block_len > 0 ){

            uint32_t copy_len = sizeof(buf);
            if( copy_len > block_len ){

                copy_len = block_len;
            }

            // block read
            spi_read( address, (uint32_t *)buf, copy_len );

            memcpy( ptr, buf, copy_len );

            ptr += copy_len;
            address += copy_len;
            block_len -= copy_len;
            len -= copy_len;
        }
    }
    else{

        // block read
        spi_read( address, ptr, block_len );
        
        address += block_len;
        ptr += block_len;    
        len -= block_len;
    }

    // unaligned read remainind bytes
    while( len > 0 ){

        *(uint8_t *)ptr = flash25_u8_read_byte( address );
        ptr++;
        address++;
        len--;        
    }
}

// read a single byte
uint8_t flash25_u8_read_byte( uint32_t address ){
    
    ASSERT( address < max_address );

    // busy wait
    BUSY_WAIT( flash25_b_busy() );

    // check if byte is in cache
    uint32_t word_address = address & ( ~0x03 );
    uint32_t byte_address = address & (  0x03 );

    if( word_address != cache_address ){

        // cache miss
        load_cache( word_address );
    }

    // return byte
    return cache_data.bytes[byte_address];
}

void flash25_v_write_enable( void ){

    // BUSY_WAIT( flash25_b_busy() );
}

void flash25_v_write_disable( void ){

    // BUSY_WAIT( flash25_b_busy() );   
}

// write a single byte to the device
void flash25_v_write_byte( uint32_t address, uint8_t byte ){

    ASSERT( address < max_address );

	// don't write to block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

        // unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

		    return;
        }

        block0_unlock = 0;
	}

    // check cache
    uint32_t word_address = address & ( ~0x03 );
    uint32_t byte_address = address & (  0x03 );

    if( word_address != cache_address ){

        // cache miss
        load_cache( word_address );
    }

    cache_data.bytes[byte_address] = byte;
    cache_dirty = TRUE;

    flash25_v_write_enable();

    flush_cache();

    BUSY_WAIT( flash25_b_busy() );

}

// write an array of data
// this function will set the write enable
// this function will NOT check for a valid address, reaching the
// end of the array, etc.
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len ){

    ASSERT( address < max_address );

    // don't write to  block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

		// unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
	}

    // this could probably be an assert, since a write of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

    // fix odd address alignment
    while( ( address % 4 ) != 0 ){

        flash25_v_write_byte( address, *(uint8_t *)ptr );

        address++;
        ptr++;
        len--;
    }

    // enable writes
    flash25_v_write_enable();

    uint32_t block_len = ( len / 4 ) * 4;

    // misaligned pointers will cause an exception!
    // if our alignment is off, we'll copy into an aligned buffer
    // and do the flash write from there.
    if( ( (uint32_t)ptr % 4 ) != 0 ){

        uint8_t buf[64] __attribute__((aligned(4)));

        while( block_len > 0 ){

            uint32_t copy_len = sizeof(buf);
            if( copy_len > block_len ){

                copy_len = block_len;
            }

            memcpy( buf, ptr, copy_len );
            
            // block write
            spi_write( address, (uint32_t *)buf, copy_len );

            ptr += copy_len;
            address += copy_len;
            block_len -= copy_len;
            len -= copy_len;
        }
    }
    else{

        // block write
        spi_write( address, (uint32_t *)ptr, block_len );

        address += block_len;
        ptr += block_len;    
        len -= block_len;
    }
    
    // unaligned write remainind bytes
    while( len > 0 ){
        
        // enable writes
        flash25_v_write_enable();

        flash25_v_write_byte( address, *(uint8_t *)ptr );
        ptr++;
        address++;
        len--;        

        BUSY_WAIT( flash25_b_busy() );
    }

    // send write disable to end command
    // this should already be disabled, but lets make certain.
    flash25_v_write_disable();
}

// erase a 4 KB block
// note that this command will wait on the busy bit to be
// clear before issuing the instruction to the device.
// since erases may take a long time, it would be best to
// check the busy bit before calling this function.
void flash25_v_erase_4k( uint32_t address ){

    ASSERT( address < max_address );

    invalidate_cache();

	// don't erase block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

		// unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
	}
    // don't erase eeprom
    else if( ( address >= FLASH_FS_EEPROM_PARTITION_START ) &&
             ( address <  ( FLASH_FS_EEPROM_PARTITION_START + FLASH_FS_EEPROM_PARTITION_SIZE ) ) ){

        // unless unlocked
        if( eeprom_erase_unlock != EEPROM_UNLOCK_CODE ){

            return;
        }
    }

    BUSY_WAIT( flash25_b_busy() );

    address += START_ADDRESS;
        
    uint32_t start = tmr_u32_get_system_time_ms();

    spi_flash_erase_sector( address / FLASH_FS_ERASE_BLOCK_SIZE );
    
    uint32_t elapsed = tmr_u32_elapsed_time_ms( start );
    if( ( elapsed > flash_erase_time ) && ( elapsed < 255 ) ){

        flash_erase_time = elapsed;
    }
}

// erase the entire array
void flash25_v_erase_chip( void ){

    uint32_t array_size = flash25_u32_capacity();

	// block 0 disabled, skip block 0
    for( uint32_t i = FLASH_FS_ERASE_BLOCK_SIZE; i < array_size; i += FLASH_FS_ERASE_BLOCK_SIZE ){

        flash25_v_write_enable();
        flash25_v_erase_4k( i );
    }
}

void flash25_v_read_device_info( flash25_device_info_t *info ){
    
    flash_id = g_rom_flashchip.device_id;

    trace_printf("Flash ID: 0x%x\r\n", flash_id);
    
    info->dev_id_2 = flash_id & 0xff;
    info->dev_id_1 = ( flash_id >> 8 ) & 0xff;
    info->mfg_id   = ( flash_id >> 16 ) & 0xff;
}

uint32_t flash25_u32_capacity( void ){

    return max_address;
}


#endif

