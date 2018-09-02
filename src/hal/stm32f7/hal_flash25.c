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

#include "system.h"
#include "flash_fs_partitions.h"
#include "flash25.h"
#include "hal_flash25.h"

#include "stm32f7xx_hal_qspi.h"

#ifdef ENABLE_FFS



extern uint16_t block0_unlock;

static QSPI_HandleTypeDef hqspi;


void hal_flash25_v_init( void ){
    
    // init qspi module
    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = 255;
    hqspi.Init.FifoThreshold = 1;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_NONE;
    hqspi.Init.FlashSize = 1;
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;

    HAL_QSPI_Init( &hqspi );


    // set CS to output
    



    // enable the write protect
    WRITE_PROTECT();

    // set the CS line to inactive
    CHIP_DISABLE();

    // send write disable
    flash25_v_write_disable();

    flash25_device_info_t info;
    flash25_v_read_device_info( &info );

    // if( info.mfg_id == FLASH_MFG_SST ){

    //     aai_write_enabled = TRUE;
    // }

    // if( aai_write_enabled ){

        // // disable busy status output on SO line
        // CHIP_ENABLE();
        // spi_u8_send( FLASH_CMD_DBUSY );
        // CHIP_DISABLE();
    // }

    // enable writes
    flash25_v_write_enable();

    // clear block protection bits
    flash25_v_write_status( 0x00 );

    // disable writes
    flash25_v_write_disable();
}

uint8_t flash25_u8_read_status( void ){

}

void flash25_v_write_status( uint8_t status ){

}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

    // busy wait
    // BUSY_WAIT( flash25_b_busy() );

    
}

// read a single byte
uint8_t flash25_u8_read_byte( uint32_t address ){

    // busy wait
    // BUSY_WAIT( flash25_b_busy() );

	uint8_t byte;

	flash25_v_read( address, &byte, sizeof(byte) );

	return byte;
}

void flash25_v_write_enable( void ){

    
}

void flash25_v_write_disable( void ){

    
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

    
}

// erase the entire array
void flash25_v_erase_chip( void ){

    uint32_t array_size = flash25_u32_capacity();

	// block 0 disabled, skip block 0
    for( uint32_t i = FLASH_FS_ERASE_BLOCK_SIZE; i < array_size; i += FLASH_FS_ERASE_BLOCK_SIZE ){

        // BUSY_WAIT( flash25_b_busy() );
        flash25_v_write_enable();
        flash25_v_erase_4k( i );
    }
}

void flash25_v_read_device_info( flash25_device_info_t *info ){

}


#endif

