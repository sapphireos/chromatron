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
// #define ENABLE_KV_NOTIFICATIONS // note this requires the network
// #define ENABLE_POWER

#define ALLOW_ASSERT_DISABLE

// memory
#define MAX_MEM_HANDLES         128
#define MEM_MAX_STACK           1536

#define MEM_HEAP_SIZE           10000

// flash fs
#define FLASH_FS_MAX_USER_FILES 16

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

#define MAX_PROGRAM_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)N_APP_PAGES ) )
#define FLASH_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)TOTAL_PAGES ) )

// asserts
// comment this out to turn off compiler asserts
#define INCLUDE_COMPILER_ASSERTS

// comment this out to turn off run-time asserts
#define INCLUDE_ASSERTS

// // loader.c LED pins
// #define LDR_LED_GREEN_DDR       DDRD
// #define LDR_LED_GREEN_PORT      PORTD
// #define LDR_LED_GREEN_PIN       5
// #define LDR_LED_YELLOW_DDR      DDRD
// #define LDR_LED_YELLOW_PORT     PORTD
// #define LDR_LED_YELLOW_PIN      6
// #define LDR_LED_RED_DDR         DDRD
// #define LDR_LED_RED_PORT        PORTD
// #define LDR_LED_RED_PIN         7

// spi.c
// SPI port settings:
#define SPI_DDR DDRB
#define SPI_PORT PORTB
#define SPI_PIN PINB
#define SPI_SS PINB0
#define SPI_MOSI PINB2
#define SPI_MISO PINB3
#define SPI_SCK PINB1

#endif
