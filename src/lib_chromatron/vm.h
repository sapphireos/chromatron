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

#ifndef _VM_H
#define _VM_H

#include "vm_core.h"
#include "ffs_global.h"

#define VM_THREAD_RATE 		1

#define VM_LAST_VM ( VM_MAX_VMS - 1 )

void vm_v_init( void );
void vm_v_start( uint8_t vm_id );
void vm_v_stop( uint8_t vm_id );
void vm_v_reset( uint8_t vm_id );

void vm_v_pause( uint8_t vm_id );
void vm_v_resume( uint8_t vm_id );

void vm_v_run_prog( char name[FFS_FILENAME_LEN], uint8_t slot );

bool vm_b_running( void );
bool vm_b_is_vm_running( uint8_t i );

void vm_v_sync( uint32_t ts, uint64_t ticks );

uint32_t vm_u32_get_sync_time( void );
uint64_t vm_u64_get_sync_tick( void );
uint32_t vm_u32_get_checkpoint( void );
uint32_t vm_u32_get_checkpoint_hash( void );

uint64_t vm_u64_get_tick( void );
uint64_t vm_u64_get_frame( void );
uint32_t vm_u32_get_data_hash( void );
uint16_t vm_u16_get_data_len( void );
int32_t* vm_i32p_get_data( void ); 
vm_state_t* vm_p_get_state( void );

#endif
