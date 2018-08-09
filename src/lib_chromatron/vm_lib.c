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

#include <stdint.h>
#include "bool.h"
#include "catbus_common.h"

#include "vm_lib.h"

int8_t vm_lib_i8_libcall_built_in( 
	catbus_hash_t32 func_hash, 
	int32_t *data,
	int32_t *result, 
	int32_t *params, 
	uint16_t param_len ){

	int32_t temp0, temp1;

	switch( func_hash ){
		case __KV__test_lib_call:
            if( param_len != 2 ){

                return 0;
            }

            *result = params[0] + params[1];
            return 0;
            break;

      	case __KV__min:
            if( param_len != 2 ){

                return 0;
            }

            *result = params[0];
            return 0;

            temp0 = data[params[0]];

            // second param is array len
            for( uint32_t i = 1; i < params[1]; i++ ){

            	temp1 = data[params[0] + i];

            	if( temp1 < temp0 ){

            		temp0 = temp1;
            	}
            }

            *result = temp0;
            return 0;
            break;

		default:
            break;
    }    
	// function not found
	return -1;
}
