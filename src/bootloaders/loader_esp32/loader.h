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


#ifndef __LOADER_H
#define __LOADER_H

#include "target.h"
#include "system.h"

#define ESP_FLASH_INTF_QIO 		0
#define ESP_FLASH_INTF_QOUT 	1
#define ESP_FLASH_INTF_DIO 		2
#define ESP_FLASH_INTF_DOUT 	3

// flash info, split in 2 nibbles:
// size: upper nibble
#define ESP_FLASH_SIZE_512K		0
#define ESP_FLASH_SIZE_256K		1
#define ESP_FLASH_SIZE_1M		2
#define ESP_FLASH_SIZE_2M		3
#define ESP_FLASH_SIZE_4M		4
#define ESP_FLASH_SIZE_8M		8
#define ESP_FLASH_SIZE_16M		9

// speed: lower nibble
#define ESP_FLASH_SPEED_40M		0
#define ESP_FLASH_SPEED_26M		1
#define ESP_FLASH_SPEED_20M		2
#define ESP_FLASH_SPEED_80M		15

#define ESP_MEM_DRAM0_ADDR		0x3FFE8000
#define ESP_MEM_DRAM0_SIZE		0x14000

#define ESP_MEM_IRAM1_ADDR		0x40100000
#define ESP_MEM_IRAM1_SIZE		0x8000

#define ESP_MEM_ICACHE_ADDR		0x40108000
#define ESP_MEM_ICACHE_SIZE		0x8000

#define ESP_MEM_IROM0_ADDR		0x40218000
#define ESP_MEM_IROM0_SIZE		0x5C000

#define LDR_VERSION_MAJOR   '1'
#define LDR_VERSION_MINOR   '0'

void loader_main( void );

void ldr_run_app( void );

void ldr_v_set_green_led( void );
void ldr_v_clear_green_led( void );
void ldr_v_set_yellow_led( void );
void ldr_v_clear_yellow_led( void );
void ldr_v_set_red_led( void );
void ldr_v_clear_red_led( void );

void ldr_v_erase_app( void );
void ldr_v_copy_partition_to_internal( void );
void ldr_v_read_partition_data( uint32_t offset, uint8_t *dest, uint16_t length );

uint32_t ldr_u32_read_partition_length( void );
uint32_t ldr_u32_read_internal_length( void );

uint16_t ldr_u16_get_internal_crc( void );
uint16_t ldr_u16_get_partition_crc( void );

#endif
