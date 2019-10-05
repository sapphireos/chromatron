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

#ifndef _VM_RUNNER_H
#define _VM_RUNNER_H

#include "list.h"
#include "gfx_lib.h"
#include "vm_core.h"
#include "wifi_cmd.h"
#include "vm_wifi_cmd.h"

#define VM_RUNNER_MAX_SIZE 		8192

uint32_t vm_u32_get_fader_time( void );
uint32_t vm_u32_get_run_time( void );
uint32_t vm_u32_get_max_cycles( void );
uint32_t vm_u32_get_active_threads( uint8_t vm_index );

void vm_v_init( void );
void vm_v_run_faders( void );

#define VM_RUN_INIT     0
#define VM_RUN_LOOP     1
#define VM_RUN_THREAD   2
#define VM_RUN_FUNC     3
int8_t vm_i8_run_vm( uint8_t vm_id, uint8_t mode );

void vm_v_reset(  uint8_t vm_index );
int8_t vm_i8_load( uint8_t *data, uint16_t len, uint16_t total_size, uint16_t offset, uint8_t vm_index );
int8_t vm_i8_start( uint32_t vm_index );
uint16_t vm_u16_get_fader_time( void );
uint16_t vm_u16_get_total_size( void );
vm_thread_t* vm_p_get_threads( uint8_t vm_index );

void vm_v_start_frame_sync( uint8_t index, wifi_msg_vm_frame_sync_t *msg, uint16_t len );
void vm_v_frame_sync_data( uint8_t index, wifi_msg_vm_sync_data_t *msg, uint16_t len );
void vm_v_frame_sync_done( uint8_t index, wifi_msg_vm_sync_done_t *msg, uint16_t len );

int8_t vm_i8_run_thread( uint8_t index, uint16_t thread_id );
int8_t vm_i8_run_func( uint8_t index, uint16_t func_addr );

void vm_v_request_frame_data( uint8_t index );

#endif
