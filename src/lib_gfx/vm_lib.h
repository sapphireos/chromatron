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


#ifndef _VM_LIB_H_
#define _VM_LIB_H_

#include "vm_core.h"

int8_t vm_lib_i8_libcall_built_in( 
	catbus_hash_t32 func_hash, 
	vm_state_t *state, 
	function_info_t *func_table,
	int32_t *pools[],
	int32_t *result, 
	int32_t *params, 
	uint16_t param_len );

#endif
