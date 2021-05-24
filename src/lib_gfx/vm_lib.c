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

#include <string.h>
#include <stdint.h>
#include "bool.h"
#include "catbus_common.h"
#include "random.h"
#include "cnt_of_array.h"
#include "vm_lib.h"
#include "io.h"
#include "timesync.h"

int8_t vm_lib_i8_libcall_built_in( 
	catbus_hash_t32 func_hash, 
    vm_state_t *state, 
	int32_t *data,
	int32_t *result, 
	int32_t *params, 
	uint16_t param_len ){

    // result is assumed to have been initialized to a default value
    // by the caller.

	int32_t temp0, temp1, array_len;

	switch( func_hash ){
        case __KV__rand:

            if( param_len == 0 ){

                *result = rnd_u16_get_int_with_seed( &state->rng_seed );

                break;
            }
            else if( param_len == 1 ){

                temp0 = data[params[0]];
                temp1 = 0;
            }
            else{

                temp0 = data[params[1]] - data[params[0]];
                temp1 = data[params[0]];
            }

            // check for divide by 0, or negative spread
            if( temp0 <= 0 ){

                // just return the offset
                *result = temp1;
                
            }
            else{
            
                *result = temp1 + ( rnd_u16_get_int_with_seed( &state->rng_seed ) % temp0 );
            }

            break;

        case __KV__yield:

            state->yield = 1;
            break;

        case __KV__delay:
            // default to yield (no delay) if no parameters
            if( param_len < 1 ){

                temp0 = 0;
            }
            else{
                // first parameter is delay time, all other params ignored
                temp0 = data[params[0]];
            }

            // bounds check
            if( temp0 < VM_MIN_DELAY ){

                temp0 = VM_MIN_DELAY;
            }

            // set up delay
            state->threads[state->current_thread].tick += temp0;

            // delay also yields
            state->yield = 1;

            break;

        case __KV__time:

            if( !time_b_is_local_sync() ){

                *result = -1;
            }
            else{

                uint32_t local_now_seconds = time_u32_local_now();

                // NTP, in the era in which this was written, will have bit 32 set
                // in the integer.  However, the FX VM that will call this function
                // is expecting a signed int32, not a uint32.  So we need to convert
                // to an epoch that makes sense.
                int32_t corrected_seconds = (int64_t)local_now_seconds - FX_NTP_EPOCH;

                if( corrected_seconds < 0 ){

                    // this is a synchronization error, the NTP timestamp is way off
                    // or we are out of epoch.

                    corrected_seconds = -1;
                }

                *result = corrected_seconds;
            }

            break;    

        case __KV__within:

            if( param_len != 2 ){

                *result = 0;
                
                break;
            }

            temp0 = data[params[0]];
            temp1 = data[params[1]];

            // cron_t *cron0 = state->

            break;
        
        case __KV__time_synced:
            
            if( time_b_is_ntp_sync() ){

                *result = 1;
            }
            else{

                *result = 0;
            }

            break;

      	case __KV__min:
            if( param_len != 2 ){

                break;
            }

            array_len = data[params[1]];

            temp0 = data[params[0]];

            // second param is array len
            for( int32_t i = 1; i < array_len; i++ ){

            	temp1 = data[params[0] + i];

            	if( temp1 < temp0 ){

            		temp0 = temp1;
            	}
            }

            *result = temp0;
            break;

        case __KV__max:
            if( param_len != 2 ){

                break;
            }

            array_len = data[params[1]];

            temp0 = data[params[0]];

            // second param is array len
            for( int32_t i = 1; i < array_len; i++ ){

                temp1 = data[params[0] + i];

                if( temp1 > temp0 ){

                    temp0 = temp1;
                }
            }

            *result = temp0;
            break;

        case __KV__sum:
            if( param_len != 2 ){

                break;
            }

            array_len = data[params[1]];

            // second param is array len
            for( int32_t i = 0; i < array_len; i++ ){

                *result += data[params[0] + i];
            }

            break;

        case __KV__avg:
            if( param_len != 2 ){

                break;
            }

            // check for divide by zero
            if( params[0] == 0 ){

                break;
            }

            array_len = data[params[1]];

            // second param is array len
            for( int32_t i = 0; i < array_len; i++ ){

                *result += data[params[0] + i];
            }

            *result /= array_len;

            break;

        case __KV__start_thread:
            if( param_len != 1 ){

                break;
            }

            // params[0] - thread addr

            // search for an empty slot
            for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

                if( state->threads[i].func_addr == 0xffff ){

                    memset( &state->threads[i], 0, sizeof(state->threads[i]) );

                    state->threads[i].func_addr = params[0];
                    state->threads[i].tick = state->tick;

                    break;
                }
            }

            break;

        case __KV__stop_thread:
            if( param_len != 1 ){

                break;
            }

            // params[0] - thread addr

            // search for matching threads
            for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

                if( state->threads[i].func_addr == params[0] ){

                    state->threads[i].func_addr = 0xffff;

                    break;
                }
            }

            break;

        case __KV__thread_running:
            if( param_len != 1 ){

                break;
            }

            // search for matching threads
            for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

                if( state->threads[i].func_addr == params[0] ){

                    *result = TRUE;

                    break;
                }
            }

            break;

        case __KV__test_lib_call:
            if( param_len != 2 ){

                break;
            }

            *result = data[params[0]] + data[params[1]];
            break;

        // perform a short strobe on the debug pin
        // this should not be used unless debugging the VM.
        // in some boards it may interfere with the pixel connections.
        case __KV__debug_strobe:
            if( param_len != 0 ){

                break;
            }

            io_v_set_mode( IO_PIN_DEBUG, IO_MODE_OUTPUT );
            
            io_v_digital_write( IO_PIN_DEBUG, TRUE );
            _delay_us( 100 );
            io_v_digital_write( IO_PIN_DEBUG, FALSE );

            break;

		default:
            // function not found
            return -1;
            break;
    }    
	
    return 0;
}
