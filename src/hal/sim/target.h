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

#ifndef _TARGET_H
#define _TARGET_H

#define HW_NAME "sim"


// modules
// #define ENABLE_WCOM
#define ENABLE_FFS
#define ENABLE_NETWORK
#define ENABLE_IP
#define ENABLE_GFX
#define ENABLE_LEGACY_VM
#define ENABLE_BATTERY
// #define ENABLE_KV_NOTIFICATIONS // note this requires the network
// #define ENABLE_POWER

#define ENABLE_LOG_TO_TRACE_PRINT // sim: trace is stdout, mirror log records to the console

#define ALLOW_ASSERT_DISABLE

// pixel config
#define MAX_PIXELS              320
#define FADER_RATE              20
#define N_PIXEL_OUTPUTS         1

// wifi
#define WIFI_MAX_IGMP           1

// vm
#define VM_ENABLE_GFX
#define VM_ENABLE_KV
#define VM_MAX_CALL_DEPTH           8
#define VM_MAX_CYCLES               16000
#define VM_MIN_DELAY                10
#define VM_MAX_THREADS              4
#define VM_MAX_VMS                  4

// memory
#define MAX_MEM_HANDLES         128
#define MEM_MAX_STACK           1536

#define MEM_HEAP_SIZE           10000

// flash fs
#define FFS_BLOCK_MAX_BLOCKS    16
#define FLASH_FS_MAX_USER_FILES 16

// flash fs partitions (in KB, must be multiples of the 4KB erase block)
#define FLASH_FS_FIRMWARE_0_SIZE_KB     64
#define FLASH_FS_FIRMWARE_1_SIZE_KB     0
#define FLASH_FS_FIRMWARE_2_SIZE_KB     0
#define FLASH_FS_EEPROM_SIZE_KB         0

// virtual fs
#define FS_MAX_VIRTUAL_FILES 16

// logging
#define LOG_MAX_BUFFER_SIZE     2048

// bootloader target settings
// page size in bytes
#define PAGE_SIZE 256
// number of pages in app section
#define N_APP_PAGES 496
// total pages
#define TOTAL_PAGES 512

// loader jump address
// #define JMP_LDR asm("jmp 0xf800") // word address, not byte address
// #define JMP_APP asm("jmp 0x0000") // word address, not byte address

#define FW_INFO_ADDRESS 0x120 // this must match the offset in the makefile!
#define FW_LENGTH_ADDRESS FW_INFO_ADDRESS
// the simulated flash is memory-mapped at this address (see
// hal_flash25.c), mirroring how real hardware exposes flash to raw reads.
#define FLASH_START 0x40000000
#define KV_CACHE_SIZE 1

#define MAX_PROGRAM_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)N_APP_PAGES ) )
#define FLASH_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)TOTAL_PAGES ) )

// asserts
// comment this out to turn off compiler asserts
#define INCLUDE_COMPILER_ASSERTS

// comment this out to turn off run-time asserts
#define INCLUDE_ASSERTS

#endif
