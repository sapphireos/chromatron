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

#ifndef _VM_WIFI_CMD_H
#define _VM_WIFI_CMD_H

#include "vm_core.h"

typedef struct __attribute__((packed)){
    uint32_t vm_id;
} wifi_msg_reset_vm_t;
#define WIFI_DATA_ID_RESET_VM          0x20

typedef struct __attribute__((packed)){
    uint32_t vm_id;
    uint8_t chunk[64];
} wifi_msg_load_vm_t;
#define WIFI_DATA_ID_LOAD_VM           0x21


typedef struct __attribute__((packed)){
    uint16_t fader_time;
    uint16_t vm_total_size;
    vm_info_t vm_info[VM_MAX_VMS];
} wifi_msg_vm_info_t;
#define WIFI_DATA_ID_VM_INFO           0x22
#define WIFI_DATA_ID_INIT_VM           0x23

#define WIFI_DATA_ID_VM_SYNC_DATA      0x24

#define WIFI_DATA_ID_RUN_VM            0x26



typedef struct __attribute__((packed)){
    uint64_t rng_seed;
    uint16_t frame_number;
    uint16_t data_index;
    uint16_t data_count;
    uint16_t padding;
} wifi_msg_vm_frame_sync_t;
#define WIFI_DATA_ID_VM_FRAME_SYNC      0x25


#define WIFI_DATA_ID_REQUEST_FRAME_SYNC 0x28

typedef struct __attribute__((packed)){
    uint16_t frame_number;
    uint8_t status;
} wifi_msg_vm_frame_sync_status_t;
#define WIFI_DATA_ID_FRAME_SYNC_STATUS  0x29


#endif



