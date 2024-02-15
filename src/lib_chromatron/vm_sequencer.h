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

#ifndef _VM_SEQUENCER_H
#define _VM_SEQUENCER_H

#define VM_SEQ_TIME_MODE_STOPPED		0
#define VM_SEQ_TIME_MODE_INTERVAL		1
#define VM_SEQ_TIME_MODE_RANDOM			2
#define VM_SEQ_TIME_MODE_MANUAL			3
#define VM_SEQ_TIME_MODE_BUTTON			4

#define VM_SEQ_SELECT_MODE_NEXT			0
#define VM_SEQ_SELECT_MODE_RANDOM		1
// #define VM_SEQ_SELECT_MODE_SHUFFLE		2


void vm_seq_v_init( void );
uint8_t vm_seq_u8_get_time_mode( void );
uint8_t vm_seq_u8_get_step( void );
void vm_seq_v_set_step( uint8_t step );

#endif