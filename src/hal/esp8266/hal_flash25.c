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

#include "system.h"
#include "timers.h"
#include "flash_fs_partitions.h"
#include "flash25.h"
#include "hal_flash25.h"
#include "hal_io.h"
#include "keyvalue.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"

static bool use_coproc;
#endif

#ifdef ENABLE_FFS

static uint32_t flash_id;

KV_SECTION_META kv_meta_t flash_id_kv[] = {
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &use_coproc, 0,  "flash_fs_on_coproc" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &flash_id,   0,  "flash_id" },
};




// FW_START_OFFSET offsets from the end of the bootloader section.
#define START_ADDRESS ( FLASH_FS_FIRMWARE_0_PARTITION_SIZE + FW_START_OFFSET )

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

static SpiFlashOpResult spi_read( uint32_t address, uint32_t *ptr, uint32_t size ){

    address += START_ADDRESS;

    #ifdef BOOTLOADER
    return SPIRead( address, ptr, size );
    #else
    return spi_flash_read( address, ptr, size );
    #endif
}

static SpiFlashOpResult spi_write( uint32_t address, uint32_t *ptr, uint32_t size ){

    address += START_ADDRESS;
    
    #ifdef BOOTLOADER
    return SPIWrite( address, ptr, size );
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

void hal_flash25_v_init( void ){

    // read max address
    #ifndef BOOTLOADER
    max_address = flash25_u32_read_capacity_from_info();

    uint32_t system_size = START_ADDRESS + ( 32 * 1024 ); // reserve space for SDK config area

    if( max_address >= ( START_ADDRESS + ( 64 *1024 ) ) ){
        
        max_address -= system_size;
    }
    else{

        trace_printf("Invalid flash size!\r\n");

        // invalid flash
        max_address = 0;
    }

    #ifdef ENABLE_COPROCESSOR
    if( max_address < 1048576 ){

        max_address = coproc_i32_call0( OPCODE_IO_FLASH25_SIZE );

        use_coproc = TRUE;

        trace_printf("Switching FFS to coprocessor\r\n");
    }
    #endif
    
    trace_printf("Flash capacity: %d System partition: %d\r\n", max_address, system_size );

    #ifdef ENABLE_COPROCESSOR
    if( use_coproc ){

        return;
    }
    #endif

    #else
    max_address = 1048576;
    #endif
    
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

static bool is_addr_firmware_bin( uint32_t address ){

    if( ( address >= FLASH_FS_FIRMWARE_0_PARTITION_START ) &&
        ( address < ( FLASH_FS_FIRMWARE_0_PARTITION_START + FLASH_FS_FIRMWARE_0_PARTITION_SIZE ) ) ){

        return TRUE;
    }

    return FALSE;
}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    ASSERT( address < max_address );

    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

    #ifdef ENABLE_COPROCESSOR
    if( use_coproc || is_addr_firmware_bin( address ) ){

        while( len > 0 ){

            uint32_t xfer_len = len;

            if( xfer_len > COPROC_FLASH_XFER_LEN ){

                xfer_len = COPROC_FLASH_XFER_LEN;
            }
        
            coproc_i32_call1( OPCODE_IO_FLASH25_ADDR, address );
            coproc_i32_call1( OPCODE_IO_FLASH25_LEN, xfer_len );
            coproc_i32_callp( OPCODE_IO_FLASH25_READ, (uint8_t *)ptr, xfer_len );

            address += xfer_len;
            ptr += xfer_len;
            len -= xfer_len;
        }

        return;
    }
    #endif

    ASSERT( ( address % 4 ) == 0 );

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

    #ifdef ENABLE_COPROCESSOR
    if( use_coproc || is_addr_firmware_bin( address ) ){
        
        uint8_t byte;    
        
        flash25_v_read( address, &byte, sizeof(byte) );
        
        return byte;
    }
    #endif

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

    #ifdef ENABLE_COPROCESSOR
    if( use_coproc || is_addr_firmware_bin( address ) ){
        
        flash25_v_write( address, &byte, sizeof(byte) );
        
        return;
    }
    #endif

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

    #ifdef ENABLE_COPROCESSOR
    if( use_coproc || is_addr_firmware_bin( address ) ){

        while( len > 0 ){

            uint32_t xfer_len = len;

            if( xfer_len > COPROC_FLASH_XFER_LEN ){

                xfer_len = COPROC_FLASH_XFER_LEN;
            }
        
            coproc_i32_call1( OPCODE_IO_FLASH25_ADDR, address );
            coproc_i32_call1( OPCODE_IO_FLASH25_LEN, xfer_len );
            coproc_i32_callv( OPCODE_IO_FLASH25_WRITE, (uint8_t *)ptr, xfer_len );

            address += xfer_len;
            ptr += xfer_len;
            len -= xfer_len;
        }
                
        return;
    }
    #endif


    ASSERT( ( address % 4 ) == 0 );

    uint32_t block_len = ( len / 4 ) * 4;

    // enable writes
    flash25_v_write_enable();

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

    #ifdef ENABLE_COPROCESSOR
    if( use_coproc || is_addr_firmware_bin( address ) ){
        
        coproc_i32_call1( OPCODE_IO_FLASH25_ADDR, address );
        coproc_i32_call0( OPCODE_IO_FLASH25_ERASE );
        
        return;
    }
    #endif


    address += START_ADDRESS;

    // trace_printf("Erase: 0x%x\r\n", address);
    #ifdef BOOTLOADER
    SPIEraseSector( address / FLASH_FS_ERASE_BLOCK_SIZE );
    #else
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

        sys_v_wdt_reset();
    }
}

void flash25_v_read_device_info( flash25_device_info_t *info ){
    #ifndef BOOTLOADER
    uint32_t id = spi_flash_get_id();

    flash_id = id;

    // trace_printf("Flash ID: 0x%x\r\n", id);
    
    info->mfg_id = id & 0xff;
    info->dev_id_1 = ( id >> 8 ) & 0xff;
    info->dev_id_2 = ( id >> 16 ) & 0xff;
    
    #endif
}

uint32_t flash25_u32_capacity( void ){

    return max_address;
}


#endif

