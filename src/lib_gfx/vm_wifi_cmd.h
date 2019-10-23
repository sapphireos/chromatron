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
#define WIFI_DATA_ID_RESET_VM          0x61

typedef struct __attribute__((packed)){
    uint32_t vm_id;
    uint16_t total_size;
    uint16_t offset;
    uint8_t chunk[128];
} wifi_msg_load_vm_t;
#define WIFI_DATA_ID_LOAD_VM           0x62

typedef struct __attribute__((packed)){
    uint32_t fader_time;
} wifi_msg_fader_info_t;
#define WIFI_DATA_ID_RUN_FADER         0x63

typedef struct __attribute__((packed)){
    uint16_t vm_id;
    int8_t status;
    uint8_t padding;
    uint16_t run_time;
    uint16_t max_cycles;
    int32_t thread_delays[VM_MAX_THREADS];
} wifi_msg_vm_info_t;
#define WIFI_DATA_ID_VM_INFO           0x64

// VM run commands
typedef struct __attribute__((packed)){
    uint32_t vm_id;
    uint16_t func_addr;
} wifi_msg_vm_run_t;
#define WIFI_DATA_ID_INIT_VM           0x65
#define WIFI_DATA_ID_RUN_VM            0x66
#define WIFI_DATA_ID_VM_RUN_FUNC       0x67
#define WIFI_DATA_ID_VM_RUN_THREAD     0x68

typedef struct __attribute__((packed)){
    uint32_t program_name_hash;
    uint16_t frame_number;
    uint16_t data_len;
    uint64_t rng_seed;
} wifi_msg_vm_frame_sync_t;
#define WIFI_DATA_ID_VM_FRAME_SYNC      0x81
#define WIFI_DATA_ID_VM_SET_FRAME_SYNC  0x82

typedef struct __attribute__((packed)){
    uint16_t offset;
    uint16_t padding;
} wifi_msg_vm_sync_data_t;
#define WIFI_DATA_ID_VM_SYNC_DATA      	0x83
#define WIFI_DATA_ID_VM_SET_SYNC_DATA   0x84
#define WIFI_MAX_SYNC_DATA 	( 244 - sizeof(wifi_msg_vm_frame_sync_t) )

#endif




