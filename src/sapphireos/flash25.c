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

25 Series SPI Flash Memory Driver

This module provides a low level SPI driver for 25 series flash memory.

*/


#include <string.h>

#include "cpu.h"

#include "system.h"
#include "spi.h"

#include "flash_fs_partitions.h"
#include "flash25.h"

#ifdef ENABLE_FFS
#include "hal_flash25.h"


#ifdef __SIM__
    #define FLASH_FS_N_ERASE_BLOCKS			128
    // array size is the size of the entire flash memory array
    #define FLASH_FS_ARRAY_SIZE				( (uint32_t)FLASH_FS_N_ERASE_BLOCKS * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE )


    #define CHIP_ENABLE()
    #define CHIP_DISABLE()

    #define WRITE_PROTECT()
    #define WRITE_UNPROTECT()

    #define AAI_STATUS()        ( 1 )

    static uint8_t array[524288];
#endif

static bool aai_write_enabled;

#define BLOCK0_UNLOCK_CODE  0x1701
static uint16_t block0_unlock;


void flash25_v_init( void ){

    #ifdef __SIM__

    flash25_v_erase_chip();

    #else

	hal_flash25_v_init();

	// enable the write protect
	WRITE_PROTECT();

	// set the CS line to inactive
	CHIP_DISABLE();

    // send write disable
    flash25_v_write_disable();

    flash25_device_info_t info;
    flash25_v_read_device_info( &info );

    if( info.mfg_id == FLASH_MFG_SST ){

        aai_write_enabled = TRUE;
    }

    if( aai_write_enabled ){

        // disable busy status output on SO line
        CHIP_ENABLE();
        spi_u8_send( FLASH_CMD_DBUSY );
        CHIP_DISABLE();
    }

	// enable writes
	flash25_v_write_enable();

	// clear block protection bits
	flash25_v_write_status( 0x00 );

    // disable writes
	flash25_v_write_disable();

    #endif
}

uint8_t flash25_u8_read_status( void ){

    #ifdef __SIM__
    return 0;
    #else
	uint8_t status;

	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_READ_STATUS );
	status = spi_u8_send( 0 );

	CHIP_DISABLE();

	return status;
    #endif
}

void flash25_v_write_status( uint8_t status ){

    #ifndef __SIM__
	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_ENABLE_WRITE_STATUS );

	CHIP_DISABLE();


	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_WRITE_STATUS );
	spi_u8_send( status );

	CHIP_DISABLE();
    #endif
}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

    // busy wait
    SAFE_BUSY_WAIT( flash25_b_busy() );

    #ifdef __SIM__
    memcpy( ptr, &array[address], len );
    #else
	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_READ );
	spi_u8_send( address >> 16 );
	spi_u8_send( address >> 8 );
	spi_u8_send( address );

    spi_v_read_block( ptr, len );

	CHIP_DISABLE();
    #endif
}

// read a single byte
uint8_t flash25_u8_read_byte( uint32_t address ){

    // busy wait
    SAFE_BUSY_WAIT( flash25_b_busy() );

	uint8_t byte;

	flash25_v_read( address, &byte, sizeof(byte) );

	return byte;
}

void flash25_v_write_enable( void ){

    #ifndef __SIM__
	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_WRITE_ENABLE );

	CHIP_DISABLE();

	WRITE_UNPROTECT();
    #endif
}

void flash25_v_write_disable( void ){

    #ifndef __SIM__
	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_WRITE_DISABLE );

	CHIP_DISABLE();

	WRITE_PROTECT();
    #endif
}

// write a single byte to the device
void flash25_v_write_byte( uint32_t address, uint8_t byte ){

	// don't write to block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

        // unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

		    return;
        }

        block0_unlock = 0;
	}

    #ifdef __SIM__
    array[address] = byte;
    #else
	// wait on busy bit in status register
	// note that if the flash chip malfunctions and never completes
	// an operation, this will cause the entire system to hang
	// until the watchdog kicks it.  this is acceptable,
	// since at that point we have a hardware failure anyway.
	SAFE_BUSY_WAIT( flash25_b_busy() );

	flash25_v_write_enable();

	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_WRITE_BYTE );
	spi_u8_send( address >> 16 );
	spi_u8_send( address >> 8 );
	spi_u8_send( address );

	spi_u8_send( byte );

	CHIP_DISABLE();
    #endif
}

// write an array of data
// this function will set the write enable
// this function will NOT check for a valid address, reaching the
// end of the array, etc.
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len ){

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

    #ifdef __SIM__
    memcpy( &array[address], ptr, len );
    #else

    if( aai_write_enabled ){

        // check if odd address
        if( ( address & 1 ) != 0 ){

            flash25_v_write_byte( address, *(uint8_t *)ptr );

            address++;
            ptr++;
            len--;
        }

        if( len > 1 ){

            // busy wait
            SAFE_BUSY_WAIT( flash25_b_busy() );

            // enable writes
            flash25_v_write_enable();

            // enable busy status output on SO line
            CHIP_ENABLE();
            spi_u8_send( FLASH_CMD_EBUSY );
            CHIP_DISABLE();

            // use autoincrement write command
            CHIP_ENABLE();

            spi_u8_send( FLASH_CMD_AAI_WRITE );
            spi_u8_send( address >> 16 );
            spi_u8_send( address >> 8 );
            spi_u8_send( address );

            // write two bytes
            spi_u8_send( *(uint8_t *)ptr  );
            spi_u8_send( *(uint8_t *)(ptr + 1) );

            CHIP_DISABLE(); // this will begin the program cycle

            ptr += 2;
            len -= 2;
            address += 2;

            while( len > 1 ){

                // poll status output
                CHIP_ENABLE();
                _delay_us(FLASH_WAIT_DELAY_US);
                SAFE_BUSY_WAIT( AAI_STATUS() == 0 );
                CHIP_DISABLE();

                CHIP_ENABLE();

                // send command
                spi_u8_send( FLASH_CMD_AAI_WRITE );

                // write two bytes
                spi_u8_send( *(uint8_t *)ptr  );
                spi_u8_send( *(uint8_t *)(ptr + 1) );

                CHIP_DISABLE(); // this will begin the program cycle

                ptr += 2;
                len -= 2;
                address += 2;
            }

            // poll status output
            CHIP_ENABLE();
            _delay_us(FLASH_WAIT_DELAY_US);
            SAFE_BUSY_WAIT( AAI_STATUS() == 0 );
            CHIP_DISABLE();

            // send write disable to end command
            flash25_v_write_disable();

            // disable busy status output on SO line
            CHIP_ENABLE();
            spi_u8_send( FLASH_CMD_DBUSY );
            CHIP_DISABLE();
        }

        // check if there is data left
        // this also handles the case where there is only one byte to be written
        if( len == 1 ){

            flash25_v_write_byte( address, *(uint8_t *)ptr );
        }
    }
    // non-AAI write
    else{

        while( len > 0 ){

            // compute page data
            uint16_t page_len = 256 - ( address & 0xff );

            if( page_len > len ){

                page_len = len;
            }

            // enable writes
            flash25_v_write_enable();

            CHIP_ENABLE();

            // set up command and address
            spi_u8_send( FLASH_CMD_WRITE_BYTE );
        	spi_u8_send( address >> 16 );
        	spi_u8_send( address >> 8 );
        	spi_u8_send( address );

            // write page data
            spi_v_write_block( (uint8_t *)ptr, page_len );

        	CHIP_DISABLE();

            address += page_len;
            ptr += page_len;
            len -= page_len;

            // writes will automatically be disabled following completion
            // of the write.

            SAFE_BUSY_WAIT( flash25_b_busy() );
        }

        // send write disable to end command
        // this should already be disabled, but lets make certain.
        flash25_v_write_disable();
    }
    #endif
}

// erase a 4 KB block
// note that this command will wait on the busy bit to be
// clear before issuing the instruction to the device.
// since erases may take a long time, it would be best to
// check the busy bit before calling this function.
void flash25_v_erase_4k( uint32_t address ){

	// don't erase block 0
	if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

		// unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
	}

    #ifdef __SIM__
    memset( &array[address], 0xff, 4096 );
    #else
	SAFE_BUSY_WAIT( flash25_b_busy() );

	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_ERASE_BLOCK_4K );
	spi_u8_send( address >> 16 );
	spi_u8_send( address >> 8 );
	spi_u8_send( address );

	CHIP_DISABLE();

    #endif
}

// erase the entire array
void flash25_v_erase_chip( void ){

    #ifdef __SIM__

    memset( &array[FLASH_FS_ERASE_BLOCK_SIZE], 0xff, ( sizeof(array) - FLASH_FS_ERASE_BLOCK_SIZE ) );

    #else

	// #ifndef FLASH_ENABLE_BLOCK_0

    uint32_t array_size = flash25_u32_capacity();

	// block 0 disabled, skip block 0
    for( uint32_t i = FLASH_FS_ERASE_BLOCK_SIZE; i < array_size; i += FLASH_FS_ERASE_BLOCK_SIZE ){

        SAFE_BUSY_WAIT( flash25_b_busy() );
        flash25_v_write_enable();
        flash25_v_erase_4k( i );
    }
 //    #else
	// // block 0 enabled, we can use chip erase command

	// SAFE_BUSY_WAIT( flash25_b_busy() );

	// CHIP_ENABLE();

	// spi_u8_send( FLASH_CMD_CHIP_ERASE );

	// CHIP_DISABLE();

	// #endif

    #endif
}

void flash25_v_read_device_info( flash25_device_info_t *info ){

    #ifdef __SIM__

    info->mfg_id = FLASH_MFG_SST;
	info->dev_id_1 = FLASH_DEV_ID1_SST25;
	info->dev_id_2 = FLASH_DEV_ID2_SST25_4MBIT;

    #else
	CHIP_ENABLE();

	spi_u8_send( FLASH_CMD_READ_ID );

	info->mfg_id = spi_u8_send( 0 );
	info->dev_id_1 = spi_u8_send( 0 );
	info->dev_id_2 = spi_u8_send( 0 );

	CHIP_DISABLE();
	#endif
}

uint8_t flash25_u8_read_mfg_id( void ){

	flash25_device_info_t info;

	flash25_v_read_device_info( &info );

	return info.mfg_id;
}

// returns capacity in bytes
uint32_t flash25_u32_capacity( void ){

	flash25_device_info_t info;

	flash25_v_read_device_info( &info );

	uint32_t capacity = 0;

	switch( info.mfg_id ){

        // need to make all of this a lookup table
        case FLASH_MFG_SST:

			if( info.dev_id_1 == FLASH_DEV_ID1_SST25 ){

				if( info.dev_id_2 == FLASH_DEV_ID2_SST25_4MBIT ){

					capacity = 524288;
				}
                else if( info.dev_id_2 == FLASH_DEV_ID2_SST25_8MBIT ){

					capacity = 1048576;
				}
			}

			break;

        case FLASH_MFG_WINBOND:

			if( info.dev_id_1 == FLASH_DEV_ID1_WINBOND_x30 ){

                if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_4MBIT ){

                    capacity = 524288;
                }
            }
            else if( info.dev_id_1 == FLASH_DEV_ID1_WINBOND_x40 ){

                if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_8MBIT ){

                    capacity = 1048576;
                }
                else if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_16MBIT ){

                    capacity = 2097152;
                }
                else if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_32MBIT ){

                    capacity = 4194304;
                }
                else if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_64MBIT ){

                    capacity = 8388608;
                }
            }

            break;

        case FLASH_MFG_CYPRESS:

            if( info.dev_id_2 == FLASH_DEV_ID2_CYPRESS_16MBIT ){

                capacity = 2097152;
            }

            break;

		default:
			break;
	}

	return capacity;
}

void flash25_v_unlock_block0( void ){

    block0_unlock = BLOCK0_UNLOCK_CODE;
}

#endif
