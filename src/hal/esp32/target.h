/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "esp_task.h"

// modules
#define ENABLE_CATBUS_LINK
#define ENABLE_TIME_SYNC
#define ENABLE_WIFI
#define ENABLE_FFS
#define ENABLE_NETWORK
// #define ENABLE_IP
// #define ENABLE_POWER
#define ENABLE_USB_UDP_TRANSPORT
// #define ENABLE_USB
#define ENABLE_MSGFLOW
#define ENABLE_SERVICES

#define ENABLE_TRACE

// wifi
#define WIFI_MAX_PORTS          16
#define WIFI_MAX_IGMP           8

// pixel config
#define FADER_RATE              20
#define MAX_PIXELS              1024
#define N_PIXEL_OUTPUTS         1
#define PIXEL_SPI_CHANNEL		0

// if defined, places pixel buffers in malloc region
#define PIXEL_USE_MALLOC

// VM config
#define VM_ENABLE_GFX
#define VM_ENABLE_KV
#define VM_ENABLE_CATBUS
#define VM_MAX_IMAGE_SIZE   	8192
#define VM_MAX_CYCLES       	16000
#define VM_MIN_DELAY                10
#define VM_MAX_VMS                  4
#define VM_MAX_CALL_DEPTH           8
#define VM_MAX_THREADS              8

// KV
#define KV_CACHE_SIZE 16

// list
// #define ENABLE_LIST_ATOMIC

// memory
#define MAX_MEM_HANDLES         512
#define MEM_MAX_STACK           4096
#define MEM_HEAP_SIZE			65535

// flash fs
// maximum number of blocks the FS can handle
#define FFS_BLOCK_MAX_BLOCKS 1024

#define FLASH_FS_MAX_USER_FILES 64

#define FFS_ALIGN32

// virtual fs
#define FS_MAX_VIRTUAL_FILES 32

#define FLASH_FS_FIRMWARE_0_SIZE_KB     1024
#define FLASH_FS_FIRMWARE_1_SIZE_KB     0
#define FLASH_FS_FIRMWARE_2_SIZE_KB     0
#define FLASH_FS_EEPROM_SIZE_KB     	128

// logging
#define LOG_MAX_BUFFER_SIZE     2048
#define ENABLE_LOG_TO_TRACE_PRINT

// msgflow
#define MSGFLOW_MAX_Q_SIZE      8192
#define MSGFLOW_MAX_Q_MSGS      128

// ARP
#define ARP_GRATUITOUS_INTERVAL 16

// leave one block at end for bootloader information
#define FW_MAX_SIZE ( FLASH_FS_FIRMWARE_0_SIZE_KB * 1024  - FLASH_FS_ERASE_BLOCK_SIZE )

#define BOOTLOADER_INFO_BLOCK ( FLASH_FS_FIRMWARE_0_PARTITION_START + FW_MAX_SIZE )

#define FW_SPI_START_OFFSET 0x00010000
#define FLASH_START         0x400d0000
#define FW_INFO_ADDRESS     0x00000018
#define FW_LENGTH_ADDRESS   FW_INFO_ADDRESS
#define FW_LOAD_ADDR        ( FLASH_START + FW_INFO_ADDRESS )

// asserts
// comment this out to turn off compiler asserts
#define INCLUDE_COMPILER_ASSERTS

// comment this out to turn off run-time asserts
#define INCLUDE_ASSERTS



// recovery mode
#define DISABLE_SAFE_MODE

#define DISABLE_RECOVERY_MODE

// #define HALT_ON_ASSERT

#endif
