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

// the ESP8266 can only access flash on a 32 bit alignment, so 
// we are doing a simple word cache.
static uint32_t cache_data;
static uint32_t cache_address;
static bool cache_dirty;

// typedef enum{
//     CACHE_INVALID,
    // CACHE_VALID,
//     CACHE_DIRTY,
// } cache_state_t;

// static cache_state_t cache_state;


static void flush_cache( void ){

    // cache is clean, nothing to do
    if( !cache_dirty ){

        return;
    }

    // commit to flash
    spi_flash_write( cache_address, &cache_data, sizeof(cache_data) );

    // cache is now valid
    cache_dirty = FALSE;
}

static void load_cache( uint32_t address ){

    if( cache_dirty ){

        flush_cache();
    }

    spi_flash_read( address, &cache_data, sizeof(cache_data) );
    cache_address = address;
}

void hal_flash25_v_init( void ){

    // read max address
    uint32_t stuff = flash25_u32_read_capacity_from_info();
    // stuff = flash25_u32_read_capacity_from_info();
    
    
    max_address = stuff;

    trace_printf("%d\r\n", stuff);

    uint32_t temp2 = 0;
    spi_flash_erase_sector(2*1048576 / 4096);
    
    spi_flash_read(2*1048576, &temp2, sizeof(temp2));    
    trace_printf("%x\r\n", temp2);


    uint32_t temp = 0x12345678;
    spi_flash_write(2*1048576, &temp, sizeof(temp));

    spi_flash_read(2*1048576, &temp2, sizeof(temp2));    
    trace_printf("%x\r\n", temp2);

    

    // enable writes
    flash25_v_write_enable();

    // clear block protection bits
    flash25_v_write_status( 0x00 );

    // disable writes
    flash25_v_write_disable();

    // init cache so we start with valid data
    load_cache( 0 );
}

uint8_t flash25_u8_read_status( void ){

    uint8_t status = 0;

    return status;
}

void flash25_v_write_status( uint8_t status ){


}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    // ASSERT( address < max_address );

    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

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

    // ASSERT( address < max_address );

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
    return ( uint8_t )( cache_data >> ( 8 * byte_address ) );
}

void flash25_v_write_enable( void ){

    // BUSY_WAIT( flash25_b_busy() );
}

void flash25_v_write_disable( void ){

    // BUSY_WAIT( flash25_b_busy() );   
}

// write a single byte to the device
void flash25_v_write_byte( uint32_t address, uint8_t byte ){

    // ASSERT( address < max_address );

	// don't write to block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

        // unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

		    return;
        }

        block0_unlock = 0;
	}

  

    flash25_v_write_enable();

    BUSY_WAIT( flash25_b_busy() );

}

// write an array of data
// this function will set the write enable
// this function will NOT check for a valid address, reaching the
// end of the array, etc.
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len ){

    // ASSERT( address < max_address );

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

 
    while( len > 0 ){

        // compute page data
        uint16_t page_len = 256 - ( address & 0xff );

        if( page_len > len ){

            page_len = len;
        }

        // enable writes
        flash25_v_write_enable();


        // do write


        
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

    // ASSERT( address < max_address );

	// don't erase block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

		// unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
	}

    BUSY_WAIT( flash25_b_busy() );

    spi_flash_erase_sector( address / FLASH_FS_ERASE_BLOCK_SIZE );
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
    
    uint32_t id = spi_flash_get_id();

    trace_printf("Flash ID: 0x%x\r\n", id);

    info->mfg_id = FLASH_MFG_ESP12;
    info->dev_id_1 = FLASH_DEV_ID1_ESP12;
    info->dev_id_2 = FLASH_DEV_ID2_ESP12_32MBIT;
}

uint32_t flash25_u32_capacity( void ){

    return max_address;
}


#endif

