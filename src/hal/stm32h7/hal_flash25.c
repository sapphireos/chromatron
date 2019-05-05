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

#include "stm32h7xx_hal_qspi.h"

#ifdef ENABLE_FFS



extern uint16_t block0_unlock;

static QSPI_HandleTypeDef hqspi;

static uint32_t max_address;


void hal_flash25_v_init( void ){

    __HAL_RCC_QSPI_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;

    #ifdef BOARD_CHROMATRONX
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    #else

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = FSPI_SCK_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(FSPI_SCK_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = FSPI_IO0_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(FSPI_IO0_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = FSPI_IO1_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(FSPI_IO1_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = FSPI_CS_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(FSPI_CS_PORT, &GPIO_InitStruct);
    #endif

    
    // init qspi module
    hqspi.Instance                  = QUADSPI;
    //hqspi.Init.ClockPrescaler       = 3; // 50.0 MHz (at AHB 200 MHz) 
    hqspi.Init.ClockPrescaler       = 1; // 100.0 MHz (at AHB 200 MHz) 
    hqspi.Init.FifoThreshold        = 1;
    hqspi.Init.SampleShifting       = QSPI_SAMPLE_SHIFTING_NONE;
    hqspi.Init.FlashSize            = 24;
    hqspi.Init.ChipSelectHighTime   = QSPI_CS_HIGH_TIME_8_CYCLE;
    hqspi.Init.ClockMode            = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID              = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash            = QSPI_DUALFLASH_DISABLE;

    HAL_QSPI_Init( &hqspi );


    // read max address
    max_address = flash25_u32_read_capacity_from_info();

    // enable writes
    flash25_v_write_enable();

    // clear block protection bits
    flash25_v_write_status( 0x00 );

    // disable writes
    flash25_v_write_disable();
}

uint8_t flash25_u8_read_status( void ){

    uint8_t status = 0;

    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_READ_STATUS;
    cmd.Address             = 0;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = 0;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.NbData              = sizeof(status);
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );
    
    HAL_QSPI_Receive( &hqspi, (uint8_t *)&status, 50 );    

    return status;
}

void flash25_v_write_status( uint8_t status ){

    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_READ_STATUS;
    cmd.Address             = 0;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = 0;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.NbData              = sizeof(status);
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );
    
    HAL_QSPI_Transmit( &hqspi, (uint8_t *)&status, 50 );    
}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    ASSERT( address < max_address );

    // this could probably be an assert, since a read of 0 is pretty useless.
    // on the other hand, it is also somewhat harmless.
    if( len == 0 ){

        return;
    }

    // busy wait
    BUSY_WAIT( flash25_b_busy() );

    
    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_FAST_READ;
    cmd.Address             = address;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = QSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 8;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.NbData              = len;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );

    HAL_QSPI_Receive( &hqspi, (uint8_t *)ptr, 50 );   
}

// read a single byte
uint8_t flash25_u8_read_byte( uint32_t address ){

    ASSERT( address < max_address );

    // busy wait
    BUSY_WAIT( flash25_b_busy() );

	uint8_t byte;

	flash25_v_read( address, &byte, sizeof(byte) );

	return byte;
}

void flash25_v_write_enable( void ){

    BUSY_WAIT( flash25_b_busy() );

    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_WRITE_ENABLE;
    cmd.Address             = 0;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = 0;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_NONE;
    cmd.NbData              = 0;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );
}

void flash25_v_write_disable( void ){

    BUSY_WAIT( flash25_b_busy() );
    
    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_WRITE_DISABLE;
    cmd.Address             = 0;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = 0;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_NONE;
    cmd.NbData              = 0;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );
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

    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_WRITE_BYTE;
    cmd.Address             = address;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = QSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.NbData              = 1;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    flash25_v_write_enable();

    BUSY_WAIT( flash25_b_busy() );

    HAL_QSPI_Command( &hqspi, &cmd, 50 );

    HAL_QSPI_Transmit( &hqspi, (uint8_t *)&byte, 50 );    
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

    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_WRITE_BYTE;
    cmd.Address             = address;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = QSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.NbData              = 0;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    while( len > 0 ){

        // compute page data
        uint16_t page_len = 256 - ( address & 0xff );

        if( page_len > len ){

            page_len = len;
        }

        // enable writes
        flash25_v_write_enable();


        cmd.Address     = address;
        cmd.NbData      = page_len;
        HAL_QSPI_Command( &hqspi, &cmd, 50 );
        HAL_QSPI_Transmit( &hqspi, (uint8_t *)ptr, 50 );  

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

    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_ERASE_BLOCK_4K;
    cmd.Address             = address;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = QSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_NONE;
    cmd.NbData              = 0;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );    
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
    
    QSPI_CommandTypeDef cmd;
    cmd.Instruction         = FLASH_CMD_READ_ID;
    cmd.Address             = 0;
    cmd.AlternateBytes      = 0;
    cmd.AddressSize         = 0;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.NbData              = sizeof(flash25_device_info_t);
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = 0;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command( &hqspi, &cmd, 50 );
    
    HAL_QSPI_Receive( &hqspi, (uint8_t *)info, 50 );    
}

uint32_t flash25_u32_capacity( void ){

    return max_address;
}


#endif

