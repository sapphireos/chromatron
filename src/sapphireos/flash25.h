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

#ifndef _FLASH25_H
#define _FLASH25_H

#include "system.h"
#include "flash_fs_partitions.h"

#define FLASH_WAIT_DELAY_US     1


// status register bits
#define FLASH_STATUS_BUSY			0x01
#define FLASH_STATUS_WEL			0x02 // write enable latch

// #ifdef FLASH_SST25 // these bits unique to the SST25
// 	#define FLASH_STATUS_BP0			0x04
// 	#define FLASH_STATUS_BP1			0x08
// 	#define FLASH_STATUS_BP2			0x10
// 	#define FLASH_STATUS_BP3			0x20
// 	#define FLASH_STATUS_AAI			0x40 // auto address increment
// 	#define FLASH_STATUS_BPL			0x80
// #endif
//
// #ifdef FLASH_AT25 // these bits unique the the AT25
// 	#define FLASH_STATUS_SWP0			0x04
// 	#define FLASH_STATUS_SWP1			0x08
// 	#define FLASH_STATUS_WPP			0x10
// 	#define FLASH_STATUS_EPE			0x20
// 	#define FLASH_STATUS_RES			0x40
// 	#define FLASH_STATUS_SPRL			0x80
// #endif

// instruction set
#define FLASH_CMD_READ					0x03 // this is the "low speed" read command
#define FLASH_CMD_FAST_READ				0x0B
#define FLASH_CMD_ERASE_BLOCK_4K		0x20
#define FLASH_CMD_ERASE_BLOCK_32K		0x52
#define FLASH_CMD_ERASE_BLOCK_64K		0xD8
#define FLASH_CMD_CHIP_ERASE			0x60
#define FLASH_CMD_WRITE_BYTE			0x02
#define FLASH_CMD_READ_STATUS			0x05
#define FLASH_CMD_ENABLE_WRITE_STATUS	0x50
#define FLASH_CMD_WRITE_STATUS			0x01
#define FLASH_CMD_WRITE_ENABLE			0x06
#define FLASH_CMD_WRITE_DISABLE			0x04
#define FLASH_CMD_READ_ID				0x9F
#define FLASH_CMD_AAI_WRITE				0xAD
#define FLASH_CMD_EBUSY 				0x70
#define FLASH_CMD_DBUSY 				0x80


typedef struct __attribute__((packed)){
	uint8_t mfg_id; // manufacturer's ID
	uint8_t dev_id_1;
	uint8_t dev_id_2;
} flash25_device_info_t;

#define FLASH_MFG_CYPRESS	  		    0x01 // used to be Spansion - they merged in 2015
#define FLASH_MFG_ATMEL					0x1F
#define FLASH_MFG_ST                    0x20 // ?? maybe?  found on ESP32
#define FLASH_MFG_SST					0xBF
#define FLASH_MFG_WINBOND		 	    0xEF
#define FLASH_MFG_BERG                  0xE0 // sometimes found in ESP12
#define FLASH_MFG_GIGADEVICE            0xC8
#define FLASH_MFG_BOYA_BOHONG           0x68 // not sure who this is.  found on ESP32.

#define FLASH_DEV_ID1_ATMEL             0x47
#define FLASH_DEV_ID1_SST25             0x25
#define FLASH_DEV_ID1_WINBOND_x30       0x30
#define FLASH_DEV_ID1_WINBOND_x40       0x40
#define FLASH_DEV_ID1_CYPRESS           0x40
#define FLASH_DEV_ID1_ESP12	  	    	0x99

#define FLASH_DEV_ID2_SST25_4MBIT       0x8D
#define FLASH_DEV_ID2_SST25_8MBIT       0x8E
#define FLASH_DEV_ID2_WINBOND_4MBIT     0x13
#define FLASH_DEV_ID2_WINBOND_8MBIT     0x14
#define FLASH_DEV_ID2_WINBOND_16MBIT    0x15
#define FLASH_DEV_ID2_CYPRESS_16MBIT    0x15
#define FLASH_DEV_ID2_WINBOND_32MBIT    0x16
#define FLASH_DEV_ID2_WINBOND_64MBIT    0x17

/*
ESP-12 Flash IDs:
1327328 = 0x1440E0 // Berg 8 Mbit
1458400 = 0x1640E0 // Berg 32 Mbit
1458415 = 0x1640EF // Winbond 32 Mbit
*/


#define BLOCK0_UNLOCK_CODE  0x1701
#define EEPROM_UNLOCK_CODE  0x2893 // USS Stargazer

void flash25_v_init( void );
uint8_t flash25_u8_read_status( void );
void flash25_v_write_status( uint8_t status );
void flash25_v_read( uint32_t address, void *ptr, uint32_t len );
uint8_t flash25_u8_read_byte( uint32_t address );
void flash25_v_write_enable( void );
void flash25_v_write_disable( void );
void flash25_v_write_byte( uint32_t address, uint8_t byte );
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len );
void flash25_v_erase_4k( uint32_t address );
void flash25_v_erase_chip( void );
void flash25_v_read_device_info( flash25_device_info_t *info );
uint8_t flash25_u8_read_mfg_id( void );
uint32_t flash25_u32_capacity( void );
uint32_t flash25_u32_read_capacity_from_info( void );
void flash25_v_unlock_block0( void );


static inline bool flash25_b_busy( void );

static inline bool flash25_b_busy( void ){

    #ifdef __SIM__
    return FALSE;
    #else
	return flash25_u8_read_status() & FLASH_STATUS_BUSY;
    #endif
}



#endif
