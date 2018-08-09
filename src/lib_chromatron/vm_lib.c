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
#include "random.h"

#include "vm_lib.h"

int8_t vm_lib_i8_libcall_built_in( 
	catbus_hash_t32 func_hash, 
    vm_state_t *state, 
	int32_t *data,
	int32_t *result, 
	int32_t *params, 
	uint16_t param_len ){

	int32_t temp0, temp1;

	switch( func_hash ){
        case __KV__rand:

            temp0 = data[params[1]] - data[params[0]];

            // check for divide by 0, or negative spread
            if( temp0 <= 0 ){

                // no spread, so just return the param
                temp1 = params[0];
            }
            else{

                temp1 = rnd_u16_get_int_with_seed( &state->rng_seed ) % temp0;
            }

            *result = temp1 + params[0];

            return 0;
            break;

		case __KV__test_lib_call:
            if( param_len != 2 ){

                return 0;
            }

            *result = data[params[0]] + data[params[1]];
            return 0;
            break;

      	case __KV__min:
            if( param_len != 2 ){

                return 0;
            }

            temp0 = data[params[0]];

            // second param is array len
            for( int32_t i = 1; i < params[1]; i++ ){

            	temp1 = data[params[0] + i];

            	if( temp1 < temp0 ){

            		temp0 = temp1;
            	}
            }

            *result = temp0;
            return 0;
            break;

        case __KV__max:
            if( param_len != 2 ){

                return 0;
            }

            temp0 = data[params[0]];

            // second param is array len
            for( int32_t i = 1; i < params[1]; i++ ){

                temp1 = data[params[0] + i];

                if( temp1 > temp0 ){

                    temp0 = temp1;
                }
            }

            *result = temp0;
            return 0;
            break;

        case __KV__sum:
            if( param_len != 2 ){

                return 0;
            }

            // second param is array len
            for( int32_t i = 0; i < params[1]; i++ ){

                *result += data[params[0] + i];
            }

            return 0;
            break;

        case __KV__avg:
            if( param_len != 2 ){

                return 0;
            }

            // check for divide by zero
            if( params[0] == 0 ){

                return 0;
            }

            // second param is array len
            for( int32_t i = 0; i < params[1]; i++ ){

                *result += data[params[0] + i];
            }

            *result /= params[1];

            return 0;
            break;

		default:
            break;
    }    
	// function not found
	return -1;
}
