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

#include "cpu.h"
#include "crc.h"
#include "system.h"
#include "config.h"

#include "watchdog.h"
#include "ffs_global.h"
#include "flash25.h"
#include "flash_fs_partitions.h"

#include "timers.h"
#include "threading.h"

#include "ffs_fw.h"

#ifdef ESP32
#include "esp32/rom/spi_flash.h"
#include "esp_spi_flash.h"
#include "hal_flash25.h"
#endif

static uint32_t fw_size;
#if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
static uint32_t fw_size1;
#endif
#if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
static uint32_t fw_size2;
#endif

typedef struct{
    uint32_t partition_start;
    uint32_t n_blocks;
    uint16_t i;
} fw_erase_thread_state_t;
PT_THREAD( fw_erase_thread( pt_t *pt, fw_erase_thread_state_t *i ) );


#define FAST_ERASE_N_BLOCKS 8


static void erase_fw_partition( uint8_t partition ){

    trace_printf("erasing fw partition %d\r\n", partition);

    if( partition == 0 ){

        for( uint16_t i = 0; i < FLASH_FS_FIRMWARE_0_N_BLOCKS; i++ ){

            // enable writes
            flash25_v_write_enable();

            // erase current block
            flash25_v_erase_4k( ( (uint32_t)i * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE ) + FLASH_FS_FIRMWARE_0_PARTITION_START );

            // wait for erase to complete
            BUSY_WAIT( flash25_b_busy() );

            sys_v_wdt_reset();
        }
    }

// note - cannot erase partition 1

    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( partition == 2 ){
        
        for( uint16_t i = 0; i < FLASH_FS_FIRMWARE_2_N_BLOCKS; i++ ){

            // enable writes
            flash25_v_write_enable();

            // erase current block
            flash25_v_erase_4k( ( (uint32_t)i * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE ) + FLASH_FS_FIRMWARE_2_PARTITION_START );

            // wait for erase to complete
            BUSY_WAIT( flash25_b_busy() );

            sys_v_wdt_reset();
        }
    }
    #endif
}

static void erase_start_blocks( uint8_t partition ){

    uint32_t partition_start = 0;
    
    if( partition == 0 ){

        partition_start = FLASH_FS_FIRMWARE_0_PARTITION_START;
    }
    // note - cannot erase partition 1
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( partition == 2 ){

        partition_start = FLASH_FS_FIRMWARE_2_PARTITION_START;
    }
    #endif
    else{

        ASSERT( FALSE );
    }

    for( uint16_t i = 0; i < FAST_ERASE_N_BLOCKS; i++ ){

        // enable writes
        flash25_v_write_enable();

        // erase current block
        flash25_v_erase_4k( ( (uint32_t)i * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE ) + partition_start );

        // wait for erase to complete
        BUSY_WAIT( flash25_b_busy() );

        sys_v_wdt_reset();
    }
}

int8_t ffs_fw_i8_init( void ){

    trace_printf("FFS FW init...\r\n");

    #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
    // init sizes for firmware 1
    flash25_v_read( FW_LENGTH_ADDRESS + FLASH_FS_FIRMWARE_1_PARTITION_START, &fw_size1, sizeof(fw_size1) );

    if( fw_size1 > FLASH_FS_FIRMWARE_1_PARTITION_SIZE ){

        fw_size1 = 0;
    }
    #endif

    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    fw_size2 = FLASH_FS_FIRMWARE_2_PARTITION_SIZE;
    #endif

    #ifdef ESP8266
    uint32_t sys_fw_length = sys_u32_get_fw_length(); // includes CRC
    #else
    uint32_t sys_fw_length = sys_u32_get_fw_length() + sizeof(uint16_t); // adjust for CR
    #endif
    uint32_t ext_fw_length = 0;

    // read firmware info from external flash partition
    #ifdef ESP32
    uint32_t fw_start_offset = hal_flash25_u32_get_partition_start();
    trace_printf("Partition FW_START_OFFSET 0x%0x\n", fw_start_offset);

    flash25_v_read( FLASH_FS_FIRMWARE_0_PARTITION_START + FW_START_OFFSET - FW_SPI_START_OFFSET,
                    &ext_fw_length,
                    sizeof(ext_fw_length) );
    #else
    flash25_v_read( FLASH_FS_FIRMWARE_0_PARTITION_START + FW_LENGTH_ADDRESS,
                    &ext_fw_length,
                    sizeof(ext_fw_length) );
    #endif

    #ifdef ESP8266
    ext_fw_length += sizeof(uint16_t) + 16; // adjust for CRC and MD5
    #else
    ext_fw_length += sizeof(uint16_t); // adjust for CRC
    #endif
    
    // check for invalid ext firmware length
    if( ext_fw_length > FLASH_FS_FIRMWARE_0_PARTITION_SIZE ){

        fw_size = 0;
    }
    else{

        fw_size = ext_fw_length;
    }

    trace_printf("internal  len: %d\r\n", sys_fw_length);
    trace_printf("partition len: %d\r\n", ext_fw_length);

    // bounds check
    if( fw_size > FLASH_FS_FIRMWARE_0_PARTITION_SIZE ){

        // invalid size, so we'll default to 0 
        fw_size = 0;
    }

    // do we have enough capacity for firmware partitions?
    uint32_t capacity = flash25_u32_capacity();
    uint32_t total_fw_partitions_len = 
        FLASH_FS_FIRMWARE_0_PARTITION_SIZE + 
        FLASH_FS_FIRMWARE_1_PARTITION_SIZE +
        FLASH_FS_FIRMWARE_2_PARTITION_SIZE;

    if( capacity < total_fw_partitions_len ){

        trace_printf("FFS FW not enough capacity for firmware partitions!\r\n");

        return -1;
    }

    #ifdef ESP8266 
    // esp8266 (with coprocessor) cannot copy fw to coproc
    return FFS_STATUS_OK;
    #endif

    // check CRC or if partition length is bad
    if( ffs_fw_u16_crc() != 0 ){
    // if( ( sys_fw_length != ext_fw_length ) ||
    //     ( ffs_fw_u16_crc() != 0 ) ){

        // CRC is bad

        // erase partition
        erase_fw_partition( 0 );

        trace_printf("copy firmware... len: %d\r\n", sys_fw_length);

        uint8_t buf[256] __attribute__((aligned(4)));

        // copy firmware to partition
        for( uint32_t i = 0; i < sys_fw_length; i += sizeof(buf) ){

            sys_v_wdt_reset();

            uint32_t block_len = sys_fw_length - i;

            if( block_len > sizeof(buf) ){

                block_len = sizeof(buf);
            }

            // read byte from internal flash
            #ifdef ESP32
            for( uint16_t j = 0; j < block_len; j += 4 ){
            #else
            for( uint16_t j = 0; j < block_len; j++ ){
            #endif

                #ifdef ESP32
                uint32_t temp;
                spi_flash_read( i + j + FW_SPI_START_OFFSET, &temp, sizeof(temp) );

                buf[j + 0] = ( temp >>  0 ) & 0xff;
                buf[j + 1] = ( temp >>  8 ) & 0xff;
                buf[j + 2] = ( temp >> 16 ) & 0xff;
                buf[j + 3] = ( temp >> 24 ) & 0xff;

                #else
                buf[j] = pgm_read_byte_far( i + j + FLASH_START );
                #endif
            }

            flash25_v_write( i + (uint32_t)FLASH_FS_FIRMWARE_0_PARTITION_START, buf, block_len );
        }

        fw_size = sys_fw_length;

        // recheck CRC
        if( ffs_fw_u16_crc() != 0 ){

            trace_printf("fw crc fail!!!\r\n");

            fw_size = 0;

            return FFS_STATUS_ERROR;
        }
    }

    return FFS_STATUS_OK;
}

uint16_t ffs_fw_u16_crc( void ){

    #ifdef __SIM__
    return 0;
    #endif

    trace_printf("fw crc check...\r\n");

	uint16_t crc = crc_u16_start();
	uint32_t length = fw_size;
    uint32_t address = FLASH_FS_FIRMWARE_0_PARTITION_START;

	while( length > 0 ){

        uint8_t buf[64];
        uint16_t copy_len = 64;

        if( (uint32_t)copy_len > length ){

            copy_len = length;
        }

        if( copy_len > length ){

            copy_len = length;
        }

        flash25_v_read( address, buf, copy_len );

        address += copy_len;
        length -= copy_len;

		crc = crc_u16_partial_block( crc, buf, copy_len );

		// reset watchdog timer
		sys_v_wdt_reset();
	}

    crc = crc_u16_finish( crc );

    trace_printf("fw crc: 0x%04x\r\n", crc);

	return crc;
}

// return the application length
uint32_t ffs_fw_u32_read_internal_length( void ){

    uint32_t internal_length = sys_u32_get_fw_length();

    #ifndef ESP8266
    internal_length += sizeof(uint16_t);
    #endif

    #ifdef N_APP_PAGES
    if( internal_length > ( (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE ) ){

        internal_length = (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE;
    }
    #endif

    return internal_length;
}

// uint16_t ffs_fw_u16_get_internal_crc( void ){

//     uint16_t crc = crc_u16_start();

//     uint32_t length = ffs_fw_u32_read_internal_length();

//     #ifdef ESP32
//     for( uint32_t i = 0; i < length; i += 4 ){
//     #else
//     for( uint32_t i = 0; i < length; i++ ){
//     #endif

//         #ifdef ESP32
//         uint32_t temp;
//         spi_flash_read( i + FW_START_OFFSET, &temp, sizeof(temp) );

//         crc = crc_u16_byte( crc, ( temp >>  0 ) & 0xff );
//         crc = crc_u16_byte( crc, ( temp >>  8 ) & 0xff );
//         crc = crc_u16_byte( crc, ( temp >> 16 ) & 0xff );
//         crc = crc_u16_byte( crc, ( temp >> 24 ) & 0xff );

//         #else
//         crc = crc_u16_byte( crc, pgm_read_byte_far( i + FLASH_START ) );
//         #endif

//         // reset watchdog timer
//         sys_v_wdt_reset();
//     }

//     return crc_u16_finish( crc );
// }


uint32_t ffs_fw_u32_size( uint8_t partition ){

    if( partition == 0 ){

        return fw_size;
    }
    #if FLASH_FS_FIRMWARE_1_SIZE_KB > 0
    else if( partition == 1 ){

        return fw_size1;
    }
    #endif
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else{

        return fw_size2;
    }
    #endif

    ASSERT( FALSE ); // bad partition ID

    return 0;
}

PT_THREAD( fw_erase_thread( pt_t *pt, fw_erase_thread_state_t *state ) )
{
PT_BEGIN( pt );

    // assumes FAST_ERASE_N_BLOCKS have already been erased before the thread started

    state->i = FAST_ERASE_N_BLOCKS;

    while( state->i < state->n_blocks ){

        // enable writes
        flash25_v_write_enable();

        // erase current block
        flash25_v_erase_4k( ( (uint32_t)state->i * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE ) + state->partition_start );

        // wait for erase to complete
        BUSY_WAIT( flash25_b_busy() );

        wdg_v_reset();

        state->i++;

        TMR_WAIT( pt, 4 );
    }

PT_END( pt );
}

void ffs_fw_v_erase( uint8_t partition, bool immediate ){

    fw_erase_thread_state_t thread_state;
    thread_state.i = 0;

    if( partition == 0 ){

        // check if we've already erased the file
        if( fw_size == 0 ){

            return;
        }

        thread_state.partition_start = FLASH_FS_FIRMWARE_0_PARTITION_START;
        thread_state.n_blocks        = FLASH_FS_FIRMWARE_0_N_BLOCKS;

        // clear firmware size
        fw_size = 0;
    }
    
    // note - cannot erase partition 1
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( partition == 2 ){

        // check if we've already erased the file
        if( fw_size2 == 0 ){

            return;
        }

        thread_state.partition_start = FLASH_FS_FIRMWARE_2_PARTITION_START;
        thread_state.n_blocks        = FLASH_FS_FIRMWARE_2_N_BLOCKS;

        // clear firmware size
        fw_size2 = 0;
    }
    #endif
    else{

        return;
    }

    immediate = TRUE;

    if( !immediate ){

        // try to create erase thread
        if( thread_t_create( THREAD_CAST( fw_erase_thread ),
                             PSTR("fw_erase_thread"),
                             &thread_state,
                             sizeof(thread_state) ) < 0 ){

            // erase thread failed, switch to immediate mode
            immediate = TRUE;
        }
        else{

            erase_start_blocks( partition );
        }
    }

    if( immediate ){

        erase_fw_partition( partition );
    }
}

int32_t ffs_fw_i32_read( uint8_t partition, uint32_t position, void *data, uint32_t len ){

    uint32_t read_len = FFS_STATUS_INVALID_FILE;

    if( partition == 0 ){

        // check position
        if( position > fw_size ){

            return FFS_STATUS_EOF;
        }

        // set read length
        read_len = len;

        // bounds check and limit read len to end of file
        if( ( position + read_len ) > fw_size ){

            read_len = fw_size - position;
        }

        // copy data
        flash25_v_read( position + FLASH_FS_FIRMWARE_0_PARTITION_START, data, read_len );
    }
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( partition == 2 ){

        // check position
        if( position > fw_size2 ){

            return FFS_STATUS_EOF;
        }

        // set read length
        read_len = len;

        // bounds check and limit read len to end of file
        if( ( position + read_len ) > fw_size2 ){

            read_len = fw_size2 - position;
        }

        // copy data
        flash25_v_read( position + FLASH_FS_FIRMWARE_2_PARTITION_START, data, read_len );
    }
    #endif

    // return length of data copied
    return read_len;
}


int32_t ffs_fw_i32_write( uint8_t partition, uint32_t position, const void *data, uint32_t len ){

    uint32_t write_len = FFS_STATUS_INVALID_FILE;

    if( partition == 0 ){

        // check position
        if( position > FLASH_FS_FIRMWARE_0_PARTITION_SIZE ){

            return FFS_STATUS_EOF;
        }

        // set write length
        write_len = len;

        // bounds check write length to partition size
        if( ( FLASH_FS_FIRMWARE_0_PARTITION_SIZE - position ) < write_len ){

            write_len = FLASH_FS_FIRMWARE_0_PARTITION_SIZE - position;
        }

        // NOTE!!!
        // This does not check if the section being written to has been pre-erased!
        // It is up to the user to ensure that the section has been erased before
        // writing to the firmware section!

        // copy data
        flash25_v_write( position + FLASH_FS_FIRMWARE_0_PARTITION_START, data, write_len );

        // adjust file size
        fw_size = write_len + position;
    }
    #if FLASH_FS_FIRMWARE_2_SIZE_KB > 0
    else if( partition == 2 ){

        // check position
        if( position > FLASH_FS_FIRMWARE_2_PARTITION_SIZE ){

            return FFS_STATUS_EOF;
        }

        // set write length
        write_len = len;

        // bounds check write length to partition size
        if( ( FLASH_FS_FIRMWARE_2_PARTITION_SIZE - position ) < write_len ){

            write_len = FLASH_FS_FIRMWARE_2_PARTITION_SIZE - position;
        }

        // NOTE!!!
        // This does not check if the section being written to has been pre-erased!
        // It is up to the user to ensure that the section has been erased before
        // writing to the firmware section!

        // copy data
        flash25_v_write( position + FLASH_FS_FIRMWARE_2_PARTITION_START, data, write_len );

        // adjust file size
        fw_size2 = write_len + position;
    }
    #endif

    return write_len;
}

