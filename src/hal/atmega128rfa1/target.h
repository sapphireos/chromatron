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

// #define TRACE
// #define ENABLE_LOG_TO_TRACE_PRINT

// modules
// #define ENABLE_CATBUS_LINK
// #define ENABLE_TIME_SYNC

// #define ENABLE_USB_UDP_TRANSPORT
// #define ENABLE_NETWORK

// #define ENABLE_WIFI
#define ENABLE_GFX
#define ENABLE_FFS
// #define ENABLE_IP
// #define ENABLE_POWER
// #define ENABLE_WIFI_USB_LOADER
// #define ENABLE_ESP_UPGRADE_LOADER
// #define ENABLE_COPROCESSOR


// #define ENABLE_USB

// #define DISABLE_SAFE_MODE

// recovery mode
#define DISABLE_RECOVERY_MODE

// pixel config
#define MAX_PIXELS              64
#define FADER_RATE              20
#define N_PIXEL_OUTPUTS         1

#define WIFI_MAX_IGMP           1


// VM config
// #define VM_TARGET_ESP
// #define VM_ENABLE_GFX
// #define VM_ENABLE_KV
// #define VM_ENABLE_CATBUS

// this needs to match what is in the wifi target.h!!
#define VM_MAX_VMS                  4
#define VM_MAX_CALL_DEPTH           8
#define VM_MAX_THREADS              4
#define VM_MAX_IMAGE_SIZE           4096
#define VM_MAX_CYCLES 		        8192
#define VM_MIN_DELAY				10

// wifi
#define WIFI_MAX_NETMSGS		4
// software max power limiter - per platform power limit in dbm
#define WIFI_MAX_SW_TX_POWER    17

// KV
#define KV_CACHE_SIZE 1

// list
// #define ENABLE_LIST_ATOMIC

// memory
#define MAX_MEM_HANDLES         32
#define MEM_MAX_STACK           1536

// flash fs

// maximum number of blocks the FS can handle
#define FFS_BLOCK_MAX_BLOCKS 254

#define FLASH_FS_MAX_USER_FILES 24

// virtual fs
#define FS_MAX_VIRTUAL_FILES 16

#define FLASH_FS_FIRMWARE_0_SIZE_KB     128
#define FLASH_FS_FIRMWARE_1_SIZE_KB     128
#define FLASH_FS_FIRMWARE_2_SIZE_KB     384
#define FLASH_FS_EEPROM_SIZE_KB     	0 // MUST be 0 on the Xmega target!

// logging
#define LOG_MAX_BUFFER_SIZE     512

// bootloader target settings
// page size in bytes
#define PAGE_SIZE 256
// number of pages in app section
#define N_APP_PAGES 496
// total pages
#define TOTAL_PAGES 512

#define FLASH_START 	0x00
#define FW_INFO_ADDRESS 0x1FC // this must match the offset in the makefile!
#define FW_LENGTH_ADDRESS FW_INFO_ADDRESS

#define MAX_PROGRAM_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)N_APP_PAGES ) )
#define FLASH_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)TOTAL_PAGES ) )

#define LDR_HAS_EIND

// asserts
// comment this out to turn off compiler asserts
#define INCLUDE_COMPILER_ASSERTS

// comment this out to turn off run-time asserts
#define INCLUDE_ASSERTS


// portb spi
#define SPI_DDR DDRB
#define SPI_PORT PORTB
#define SPI_PIN PINB
#define SPI_SS PINB0
#define SPI_MOSI PINB2
#define SPI_MISO PINB3
#define SPI_SCK PINB1

#define FLASH_CS_DDR DDRB
#define FLASH_CS_PORT PORTB
#define FLASH_CS_PIN 5

#define FLASH_WP_DDR DDRB
#define FLASH_WP_PORT PORTB
#define FLASH_WP_PIN 4

#endif
