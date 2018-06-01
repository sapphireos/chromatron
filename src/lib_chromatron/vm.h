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

#ifndef _VM_H
#define _VM_H

#include "vm_core.h"
#include "vm_wifi_cmd.h"

#define VM_MAX_FILENAME_LEN 	32
#define VM_MAX_SUBSCRIBED_DB	32 	

#define VM_KV_TAG_START   		0x30


void vm_v_init( void );
void vm_v_start( void );
void vm_v_stop( void );
void vm_v_reset( void );

void vm_v_received_info( wifi_msg_vm_info_t *msg );

bool vm_b_running( void );

#endif
