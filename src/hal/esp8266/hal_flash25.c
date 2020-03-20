// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifdef ENABLE_FFS



extern uint16_t block0_unlock;

static uint32_t max_address;
static uint32_t start_address;

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

static void flush_cache( void ){

    // cache is clean, nothing to do
    if( !cache_dirty ){

        return;
    }

    #ifndef BOOTLOADER
    // commit to flash
    spi_flash_write( cache_address, &cache_data.word, sizeof(cache_data) );
    #endif

    // cache is now valid
    cache_dirty = FALSE;
}

static void load_cache( uint32_t address ){

    if( cache_dirty ){

        flush_cache();
    }

    #ifndef BOOTLOADER
    spi_flash_read( address, &cache_data.word, sizeof(cache_data) );
    #endif
    
    cache_address = address;
}

void hal_flash25_v_init( void ){

    // read max address
    max_address = flash25_u32_read_capacity_from_info();

    if( max_address > ( 2 * 1048576 ) ){

        start_address = ( FLASH_FS_FIRMWARE_0_SIZE_KB * 1024 );
        max_address -= start_address;

        max_address -= ( 32 * 1024 );
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

    // init cache so we start with valid data
    load_cache( start_address );
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

    // don't need to do this with the simple algorithm! flash25_u8_read_byte will offset.
    // address += start_address;

    // busy wait
    BUSY_WAIT( flash25_b_busy() );

    // we can optimize this later, for now, we do just a simple byte read
    // through the cache
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

    address += start_address;

    // check if byte is in cache
    uint32_t word_address = address & ( ~0x03 );
    uint32_t byte_address = address & (  0x03 );

    if( word_address != cache_address ){

        // cache miss
        load_cache( word_address );
    }

    // return byte
    return cache_data.bytes[3 - byte_address];
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

    address += start_address;

    // check cache
    uint32_t word_address = address & ( ~0x03 );
    uint32_t byte_address = address & (  0x03 );

    if( word_address != cache_address ){

        // cache miss
        load_cache( word_address );
    }

    cache_data.bytes[3 - byte_address] = byte;
    cache_dirty = TRUE;

    flash25_v_write_enable();

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

    // don't need to do this with the simple algorithm! flash25_v_write_byte will offset.
    // address += start_address;


    // this could probably be an assert, since a write of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

 
    while( len > 0 ){

        // compute page data
        // uint16_t page_len = 256 - ( address & 0xff );

        // if( page_len > len ){

        //     page_len = len;
        // }
        uint16_t page_len = 1;

        // enable writes
        flash25_v_write_enable();


        // do write
        flash25_v_write_byte( address, *(uint8_t *)ptr );

        
        address += page_len;
        ptr += page_len;
        len -= page_len;

        // writes will automatically be disabled following completion
        // of the write.

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

	// don't erase block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

		// unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
	}

    BUSY_WAIT( flash25_b_busy() );

    address += start_address;

    // trace_printf("Erase: 0x%x\r\n", address);
    #ifndef BOOTLOADER
    spi_flash_erase_sector( address / FLASH_FS_ERASE_BLOCK_SIZE );
    #endif
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
    #ifndef BOOTLOADER
    uint32_t id = spi_flash_get_id();

    // trace_printf("Flash ID: 0x%x\r\n", id);

    info->mfg_id = FLASH_MFG_WINBOND; // assume winbond
    info->dev_id_1 = ( id >> 8 ) & 0xff;
    info->dev_id_2 = ( id >> 16 ) & 0xff;
    #endif
}

uint32_t flash25_u32_capacity( void ){

    return max_address;
}


#endif

