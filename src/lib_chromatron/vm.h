// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#define VM_THREAD_RATE 		1

void vm_v_init( void );
void vm_v_start( void );
void vm_v_stop( void );
void vm_v_reset( void );

bool vm_b_running( void );
bool vm_b_is_vm_running( uint8_t i );

void vm_v_sync( uint32_t ts, uint64_t ticks );

uint16_t vm_u16_get_data_len( void );
uint32_t vm_u32_get_prog_hash( void );
uint64_t vm_u64_get_ticks( void );
uint64_t vm_u64_get_rng_seed( void );
uint8_t* vm_u8p_get_data( void ); 

#endif
