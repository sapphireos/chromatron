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

#ifndef _VM_WIFI_CMD_H
#define _VM_WIFI_CMD_H

#include "vm_core.h"

// gfx param data is defined in gfx_lib.h
#define WIFI_DATA_ID_GFX_PARAMS         0x71

typedef struct __attribute__((packed)){
    uint16_t r;
    uint16_t g;
    uint16_t b;
} wifi_msg_rgb_pix0_t;
#define WIFI_DATA_ID_RGB_PIX0           0x72

typedef struct __attribute__((packed)){
    uint16_t index;
    uint8_t count;
    uint8_t rgbd_array[WIFI_RGB_DATA_N_PIXELS * 4];
} wifi_msg_rgb_array_t;
#define WIFI_DATA_ID_RGB_ARRAY          0x73

typedef struct __attribute__((packed)){
    uint16_t index;
    uint8_t count;
    uint8_t padding;
    uint8_t hsv_array[WIFI_HSV_DATA_N_PIXELS * 6];
} wifi_msg_hsv_array_t;
#define WIFI_DATA_ID_HSV_ARRAY          0x74


typedef struct __attribute__((packed)){
    uint32_t vm_id;
} wifi_msg_reset_vm_t;
#define WIFI_DATA_ID_RESET_VM          0x81

typedef struct __attribute__((packed)){
    uint32_t vm_id;
    uint16_t total_size;
    uint16_t offset;
    uint8_t chunk[256];
} wifi_msg_load_vm_t;
#define WIFI_DATA_ID_LOAD_VM           0x82


#define WIFI_DATA_ID_RUN_FADER         0x83

typedef struct __attribute__((packed)){
    uint16_t vm_id;
    uint16_t fader_time;
    vm_info_t vm_info;
    uint32_t changed_hashes[8];
    uint32_t active_threads;
    int32_t thread_delay;
} wifi_msg_vm_info_t;
#define WIFI_DATA_ID_VM_INFO           0x84

// VM run commands
typedef struct __attribute__((packed)){
    uint32_t vm_id;
    uint16_t func_addr;
} wifi_msg_vm_run_t;
#define WIFI_DATA_ID_INIT_VM           0x85
#define WIFI_DATA_ID_RUN_VM            0x86
#define WIFI_DATA_ID_VM_RUN_FUNC       0x87

typedef struct __attribute__((packed)){
    uint32_t program_name_hash;
    uint16_t frame_number;
    uint16_t data_len;
    uint64_t rng_seed;
} wifi_msg_vm_frame_sync_t;
#define WIFI_DATA_ID_VM_FRAME_SYNC      0x88

typedef struct __attribute__((packed)){
    uint16_t offset;
    uint16_t padding;
} wifi_msg_vm_sync_data_t;
#define WIFI_DATA_ID_VM_SYNC_DATA      	0x89
#define WIFI_MAX_SYNC_DATA 	( 244 - sizeof(wifi_msg_vm_frame_sync_t) )

typedef struct __attribute__((packed)){
    uint32_t hash;
} wifi_msg_vm_sync_done_t;
#define WIFI_DATA_ID_VM_SYNC_DONE      	0x8A

#define WIFI_DATA_ID_REQUEST_FRAME_SYNC 0x8B

#endif



