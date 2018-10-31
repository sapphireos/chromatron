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

#ifndef _TARGET_H
#define _TARGET_H

#define HW_NAME "STM32F7"


// modules
#define ENABLE_CATBUS_LINK
#define ENABLE_TIME_SYNC
// #define ENABLE_USB_UDP_TRANSPORT
#define ENABLE_WIFI
#define ENABLE_FFS
#define ENABLE_NETWORK
// #define ENABLE_IP
// #define ENABLE_POWER
// #define ENABLE_USB
// #define ENABLE_WIFI_USB_LOADER

// list
// #define ENABLE_LIST_ATOMIC

// memory
#define MAX_MEM_HANDLES         512
#define MEM_MAX_STACK           4096
#define MEM_HEAP_SIZE			65535

// flash fs
// maximum number of blocks the FS can handle
#define FFS_BLOCK_MAX_BLOCKS 16384

#define FLASH_FS_MAX_USER_FILES 64

// virtual fs
#define FS_MAX_VIRTUAL_FILES 32

#define FLASH_FS_FIRMWARE_0_SIZE_KB     384
#define FLASH_FS_FIRMWARE_1_SIZE_KB     384
#define FLASH_FS_FIRMWARE_2_SIZE_KB     512
#define FLASH_FS_EEPROM_SIZE_KB     	32

// logging
#define LOG_MAX_BUFFER_SIZE     2048

// bootloader target settings
// page size in bytes
#define PAGE_SIZE 1024
// number of pages in app section
#define N_APP_PAGES 480
// total pages
#define TOTAL_PAGES ( N_APP_PAGES + 32 )


#define FLASH_START 	0x08000000
// #define FW_INFO_ADDRESS 0x00008200 // this must match the offset in the makefile!
#define FW_INFO_ADDRESS 0x00000200 // this must match the offset in the makefile!
#define FW_LENGTH_ADDRESS 0x00000200

#define MAX_PROGRAM_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)N_APP_PAGES ) )
#define FLASH_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)TOTAL_PAGES ) )


// asserts
// comment this out to turn off compiler asserts
#define INCLUDE_COMPILER_ASSERTS

// comment this out to turn off run-time asserts
#define INCLUDE_ASSERTS


#endif
