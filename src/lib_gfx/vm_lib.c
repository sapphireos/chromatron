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
#include "sapphire.h"
#include "bool.h"
#include "catbus_common.h"
#include "random.h"
#include "cnt_of_array.h"
#include "vm_lib.h"
#include "io.h"
#include "gfx_lib.h"
#include "pixel_mapper.h"

int8_t vm_lib_i8_libcall_built_in( 
	catbus_hash_t32 func_hash, 
    vm_state_t *state, 
	int32_t *result, 
	int32_t *params, 
	uint16_t param_len ){

    // result is assumed to have been initialized to a default value
    // by the caller.

	int32_t temp0, temp1, array_len;

    #ifdef ENABLE_PIXEL_MAPPER
    int32_t x, y, z, index, h, s, v, size;
    #endif

    *result = 0;

	switch( func_hash ){
        case __KV__rand:

            if( param_len == 0 ){

                *result = rnd_u16_get_int_with_seed( &state->rng_seed );

                break;
            }
            else if( param_len == 1 ){

                temp0 = params[0];
                temp1 = 0;
            }
            else{

                temp0 = params[1] - params[0];
                temp1 = params[0];
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

		case __KV__test_lib_call:
            if( param_len == 0 ){

                *result = 1;
            }
            else if( param_len == 1 ){

                *result = params[0] + 1;
            }
            else if( param_len == 2 ){

                *result = params[0] + params[1];
            }
            else if( param_len == 3 ){

                *result = params[0] + params[1] + params[2];
            }
            else if( param_len == 4 ){

                *result = params[0] + params[1] + params[2] + params[3];
            }
            
            break;

      	// case __KV__min:
       //      if( param_len != 2 ){

       //          break;
       //      }

       //      array_len = params[1];

       //      temp0 = params[0];

       //      // second param is array len
       //      for( int32_t i = 1; i < array_len; i++ ){

       //      	temp1 = params[0] + i;

       //      	if( temp1 < temp0 ){

       //      		temp0 = temp1;
       //      	}
       //      }

       //      *result = temp0;
       //      break;

       //  case __KV__max:
       //      if( param_len != 2 ){

       //          break;
       //      }

       //      array_len = params[1];

       //      temp0 = params[0];

       //      // second param is array len
       //      for( int32_t i = 1; i < array_len; i++ ){

       //          temp1 = params[0] + i;

       //          if( temp1 > temp0 ){

       //              temp0 = temp1;
       //          }
       //      }

       //      *result = temp0;
       //      break;

       //  case __KV__sum:
       //      if( param_len != 2 ){

       //          break;
       //      }

       //      array_len = params[1];

       //      // second param is array len
       //      for( int32_t i = 0; i < array_len; i++ ){

       //          *result += params[0] + i;
       //      }

       //      break;

       //  case __KV__avg:
       //      if( param_len != 2 ){

       //          break;
       //      }

       //      // check for divide by zero
       //      if( params[0] == 0 ){

       //          break;
       //      }

       //      array_len = params[1];

       //      // second param is array len
       //      for( int32_t i = 0; i < array_len; i++ ){

       //          *result += params[0] + i;
       //      }

       //      *result /= array_len;

       //      break;

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
                temp0 = params[0];
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

        // perform a short strobe on the debug pin
        // this should not be used unless debugging the VM.
        // in some boards it may interfere with the pixel connections.
        case __KV__debug_strobe:
            if( param_len != 0 ){

                break;
            }

            #ifdef IO_PIN_DEBUG
            io_v_set_mode( IO_PIN_DEBUG, IO_MODE_OUTPUT );
            
            io_v_digital_write( IO_PIN_DEBUG, TRUE );
            _delay_us( 100 );
            io_v_digital_write( IO_PIN_DEBUG, FALSE );
            #endif

            break;

        case __KV__io_set_mode:
            if( param_len == 2 ){

                io_v_set_mode( params[0], params[1] );
            }
            
            break;

        case __KV__io_digital_write:
            if( param_len == 2 ){

                io_v_digital_write( params[0], params[1] );
            }
            
            break;

        case __KV__io_digital_read:
            if( param_len == 1 ){

                *result = io_b_digital_read( params[0] );
            }
            
            break;

        case __KV__adc_read_raw:
            if( param_len == 1 ){

                *result = adc_u16_read_raw( params[0] );
            }
            
            break;

        case __KV__adc_read_mv:
            if( param_len == 1 ){

                *result = adc_u16_read_mv( params[0] );
            }
            
            break;

        case __KV__pwm_init:
            if( param_len == 1 ){

                pwm_v_init_channel( params[0], 0 );    
            }
            else if( param_len == 2 ){

                pwm_v_init_channel( params[0], params[1] );
            }
            
            break;

        case __KV__pwm_write:
            if( param_len == 2 ){

                pwm_v_write( params[0], params[1] );
            }
            
            break;

        #ifdef ENABLE_PIXEL_MAPPER
        case __KV__map_3d:
            if( param_len != 4 ){

                break;  
            }

            index = params[0];
            x = params[1];
            y = params[2];
            z = params[3];
            
            mapper_v_map_3d( index, x, y, z );

            break;

        case __KV__draw_3d:
            if( param_len != 7 ){

                break;  
            }

            size = params[0];
            h = params[1];
            s = params[2];
            v = params[3];
            x = params[4];
            y = params[5];
            z = params[6];
            
            mapper_v_draw_3d( size, h, s, v, x, y, z );

            break;
        
        case __KV__enable_mapper:
            mapper_v_enable();
            break;

        case __KV__disable_mapper:
            mapper_v_disable();
            break;

        case __KV__reset_mapper:
            mapper_v_reset();
            break;

        #endif

        case __KV__clear:
            gfx_v_clear();
            break;


		default:
            // function not found
            return -1;
            break;
    }    
	
    return 0;
}
