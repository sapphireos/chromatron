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

#ifndef _FWINFO_H
#define _FWINFO_H

#ifndef FW_INFO_SECTION
    #define FW_INFO_SECTION
#endif


#define FW_ID_LENGTH 16
#define OS_NAME_LEN  128
#define OS_VER_LEN  16
#define FW_NAME_LEN  128
#define FW_VER_LEN  16
#define HW_NAME_LEN  32

typedef struct __attribute__((packed)){
    uint32_t fw_length;
    uint8_t fwid[FW_ID_LENGTH];
    char os_name[OS_NAME_LEN];
    char os_version[OS_VER_LEN];
    char firmware_name[FW_NAME_LEN];
    char firmware_version[FW_VER_LEN];
    char board[HW_NAME_LEN];
    uint32_t kv_index_addr;
    uint32_t kv_index_len;
} fw_info_t;

// The fw_info_t lives in the program image, not the flash file system:
// the build stamps it into the binary at FW_INFO_ADDRESS and the OS
// reads it from the mapped program flash.  The file system block space
// is a separate address space and never sees it.
//
// In the sim the program image is the host ELF itself, so the build
// stamps the struct into program rodata (sim_fwinfo_storage, see
// ExeBuilder.stamp_fwinfo) and it is read from there.
#ifdef __SIM__
    extern const fw_info_t sim_fwinfo_storage;
    #define FW_INFO_BASE ( (const uint8_t *)&sim_fwinfo_storage )
#else
    #define FW_INFO_BASE ( (const uint8_t *)( FW_INFO_ADDRESS + FLASH_START ) )
#endif


#endif