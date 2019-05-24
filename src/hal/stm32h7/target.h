/*
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
 */

#ifndef _TARGET_H
#define _TARGET_H

#define HW_NAME "STM32H7"

// #define BOARD_CHROMATRONX

// set crystal speed
#ifdef BOARD_CHROMATRONX
#define HSE_VALUE    ((int32_t)8000000)
#else
#define HSE_VALUE    ((int32_t)16000000)
#endif

// modules
#define ENABLE_CATBUS_LINK
#define ENABLE_TIME_SYNC
// #define ENABLE_USB_UDP_TRANSPORT
#define ENABLE_WIFI
#define ENABLE_FFS
#define ENABLE_NETWORK
// #define ENABLE_IP
// #define ENABLE_POWER
#define ENABLE_USB
// #define ENABLE_WIFI_USB_LOADER

// wifi
#define WIFI_MAX_NETMSGS		16

// pixel config
#define USE_HSV_BRIDGE
#define FADER_RATE              20

#ifdef BOARD_CHROMATRONX
#define MAX_PIXELS              4096
#define N_PIXEL_OUTPUTS         6
#else
#define MAX_PIXELS              1024
#define N_PIXEL_OUTPUTS         1
#endif

#define USE_GFX_LIB
#define GFX_LIB_KV_LINKAGE

// VM config
// #define VM_TARGET_ESP
#define VM_ENABLE_GFX
#define VM_ENABLE_KV
#define VM_ENABLE_CATBUS
#define VM_MAX_IMAGE_SIZE   	16384
#define VM_MAX_CYCLES       	16000
#define VM_OPTIMIZED_DECODE
#define VM_MAX_VMS                  4
#define VM_MAX_CALL_DEPTH           8
#define VM_MAX_THREADS              4


// list
// #define ENABLE_LIST_ATOMIC

// memory
#define MAX_MEM_HANDLES         512
#define MEM_MAX_STACK           8192
#define MEM_HEAP_SIZE			65535

// flash fs
// maximum number of blocks the FS can handle
#define FFS_BLOCK_MAX_BLOCKS 16384

#define FLASH_FS_MAX_USER_FILES 64

// virtual fs
#define FS_MAX_VIRTUAL_FILES 32

#define FLASH_FS_FIRMWARE_0_SIZE_KB     512
#define FLASH_FS_FIRMWARE_1_SIZE_KB     512
#define FLASH_FS_FIRMWARE_2_SIZE_KB     512
#define FLASH_FS_EEPROM_SIZE_KB     	128

// logging
#define LOG_MAX_BUFFER_SIZE     2048

// bootloader target settings
// page size in bytes
#define PAGE_SIZE 1024
// number of pages in app section
#define N_APP_PAGES 480
// total pages
#define TOTAL_PAGES ( N_APP_PAGES + 32 )


#if (defined(DEBUG) || defined(NO_BOOT)) && !defined(BOOTLOADER)
	#define FLASH_START 	( 0x08000000 + 0 )
#else
	#define FLASH_START 	( 0x08000000 + 0x20000 )
#endif

#define BOOTLOADER_FLASH_START 0x08000000

#define FW_INFO_ADDRESS 0x00000400 // this must match the offset in the makefile!
#define FW_LENGTH_ADDRESS FW_INFO_ADDRESS


#define MAX_PROGRAM_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)N_APP_PAGES ) )
#define FLASH_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)TOTAL_PAGES ) )


// asserts
// comment this out to turn off compiler asserts
#define INCLUDE_COMPILER_ASSERTS

// comment this out to turn off run-time asserts
#define INCLUDE_ASSERTS


#endif
