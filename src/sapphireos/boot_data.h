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

#ifndef _BOOT_CONSTS_H
#define _BOOT_CONSTS_H

#include "cpu.h"

// define bootloader data transfer section
// there are 8 bytes available in the bootdata section
#ifdef __SIM__
    #define BOOTDATA
#else
	#ifdef AVR
    #define BOOTDATA __attribute ((section (".noinit")))
	#else
	#define BOOTDATA __attribute ((section (".bootdata")))
	#endif
#endif

typedef uint8_t boot_mode_t8;
#define BOOT_MODE_NORMAL    0
#define BOOT_MODE_REBOOT    1
#define BOOT_MODE_FORMAT    2

typedef uint16_t loader_command_t16;
#define LDR_CMD_NONE        0x0000
#define LDR_CMD_LOAD_FW     0x2012
#define LDR_CMD_SERIAL_BOOT 0x3023
#define LDR_CMD_RECOVERY    0x4167

typedef uint8_t loader_status_t8;
#define LDR_STATUS_NORMAL               0
#define LDR_STATUS_NEW_FW               1
#define LDR_STATUS_RECOVERED_FW         2
#define LDR_STATUS_PARTITION_CRC_BAD    3
#define LDR_STATUS_RECOVERY_MODE        4

typedef struct __attribute__((packed)){
	uint16_t reboots;
	boot_mode_t8 boot_mode;
    loader_command_t16 loader_command;
    uint8_t loader_version_major;
    uint8_t loader_version_minor;
    loader_status_t8 loader_status;
} boot_data_t;


#endif
