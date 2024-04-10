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
#include "catbus_link.h"
#include "pixel_mapper.h"
#include "vm.h"
#include "vm_core.h"

#ifdef ENABLE_CONTROLLER
#include "mqtt_client.h"
#endif

#ifdef ENABLE_BATTERY
#include "battery.h"
#include "buttons.h"
#include "fuel_gauge.h"
#endif


int8_t vm_lib_i8_libcall_built_in( 
	catbus_hash_t32 func_hash, 
    vm_state_t *state, 
    function_info_t *func_table,
    int32_t *pools[],
	int32_t *result, 
	int32_t *params, 
	uint16_t param_len ){

    // result is assumed to have been initialized to a default value
    // by the caller.

	int32_t temp0, temp1, vm_id;
    int32_t *ptr;
    char *str;
    char *str2;
    vm_reference_t ref;
    uint16_t func_addr;

    #ifdef ENABLE_PIXEL_MAPPER
    int32_t x, y, z, index, h, s, v, size;
    #endif

	switch( func_hash ){
        #ifdef ENABLE_CONTROLLER
        case __KV__publish:

            // dereference to pool:
            // topic is param 0
            ref.n = params[0];
            ptr = (int32_t *)( pools[ref.ref.pool] + ref.ref.addr );
            str = (char *)ptr;

            // data is param 1
            temp0 = params[1];

            catbus_meta_t meta = {
                0, // hash,
                CATBUS_TYPE_INT32, // type,
                0, // array count
                0, // flags
                0, // reserved
            };

            *result = mqtt_client_i8_publish_data( str, &meta, &temp0, 0, FALSE );

            break;
        #endif

        case __KV__rand:

            if( param_len == 0 ){

                *result = rnd_u16_get_int_with_seed( &state->rng_seed );

                break;
            }
            else if( param_len == 1 ){

                temp0 = params[0];
                
                *result = rnd_u16_range_with_seed( &state->rng_seed, temp0 );
            }
            else{

                temp0 = params[1] - params[0];
                temp1 = params[0];

                *result = temp1 + rnd_u16_range_with_seed( &state->rng_seed, temp0 );
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

        case __KV__start_thread:
            if( param_len != 1 ){

                break;
            }

            // params[0] - function ref
            // decode reference:
            ref.n = params[0];

            // verify storage pool:
            if( ref.ref.pool != POOL_FUNCTIONS ){

                break;
            }

            func_addr = func_table[ref.ref.addr].addr;

            // search for an empty slot
            for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

                vm_thread_t *vm_thread = (vm_thread_t *)&state->threads[i];

                if( vm_thread->func_addr == 0xffff ){

                    memset( vm_thread, 0, sizeof(vm_thread_t) );

                    vm_thread->func_addr = func_addr;
                    vm_thread->tick = state->tick;

                    break;
                }
            }

            break;

        case __KV__stop_thread:
            if( param_len != 1 ){

                break;
            }

            // params[0] - function ref
            // decode reference:
            ref.n = params[0];

            // verify storage pool:
            if( ref.ref.pool != POOL_FUNCTIONS ){

                break;
            }

            func_addr = func_table[ref.ref.addr].addr;

            // search for matching threads
            for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

                vm_thread_t *vm_thread = (vm_thread_t *)&state->threads[i];

                if( vm_thread->func_addr == func_addr ){

                    vm_thread->func_addr = 0xffff;

                    break;
                }
            }

            break;

        case __KV__thread_running:
            if( param_len != 1 ){

                break;
            }

            // params[0] - function ref
            // decode reference:
            ref.n = params[0];

            // verify storage pool:
            if( ref.ref.pool != POOL_FUNCTIONS ){

                break;
            }

            func_addr = func_table[ref.ref.addr].addr;

            // search for matching threads
            for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

                vm_thread_t *vm_thread = (vm_thread_t *)&state->threads[i];

                if( vm_thread->func_addr == func_addr ){

                    *result = TRUE;

                    break;
                }
            }

            break;

        case __KV__vm_start:
            if( param_len == 0 ){

                vm_id = 0;
            }
            else{

                vm_id = params[0];
            }

            if( ( vm_id < 0 ) || ( vm_id >= VM_MAX_VMS ) ){

                break;
            }

            vm_v_start( vm_id );

            break;

        case __KV__vm_stop:
            if( param_len == 0 ){

                vm_id = 0;
            }
            else{

                vm_id = params[0];
            }

            if( ( vm_id < 0 ) || ( vm_id >= VM_MAX_VMS ) ){

                break;
            }

            vm_v_stop( vm_id );

            break;

        case __KV__vm_reset:
            if( param_len == 0 ){

                vm_id = 0;
            }
            else{

                vm_id = params[0];
            }

            if( ( vm_id < 0 ) || ( vm_id >= VM_MAX_VMS ) ){

                break;
            }

            vm_v_reset( vm_id );

            break;

        case __KV__vm_halted:
            if( param_len == 0 ){

                vm_id = 0;
            }
            else{

                vm_id = params[0];
            }

            if( ( vm_id < 0 ) || ( vm_id >= VM_MAX_VMS ) ){

                break;
            }

            *result = vm_b_halted( vm_id );

            break;


        #ifdef ENABLE_BATTERY
        case __KV__button_pressed:
            if( param_len == 0 ){

                temp0 = 0;
            }
            else{

                temp0 = params[0];
            }

            if( temp0 < 0 ){

                break;
            }

            *result = button_b_is_button_pressed( temp0 );

            break;

        case __KV__button_held:
            if( param_len == 0 ){

                temp0 = 0;
            }
            else{

                temp0 = params[0];
            }

            if( temp0 < 0 ){

                break;
            }

            *result = button_b_is_button_hold( temp0 );

            break;

        case __KV__button_released:
            if( param_len == 0 ){

                temp0 = 0;
            }
            else{

                temp0 = params[0];
            }

            if( temp0 < 0 ){

                break;
            }

            *result = button_b_is_button_released( temp0 );

            break;

        case __KV__button_hold_released:
            if( param_len == 0 ){

                temp0 = 0;
            }
            else{

                temp0 = params[0];
            }

            if( temp0 < 0 ){

                break;
            }

            *result = button_b_is_button_hold_released( temp0 );

            break;

        case __KV__batt_charge_full:

            *result = fuel_b_threshold_full_charge();

            break;

        case __KV__batt_charge_top:

            *result = fuel_b_threshold_top_charge();

            break;

        case __KV__batt_charge_mid:

            *result = fuel_b_threshold_mid_charge();

            break;

        case __KV__batt_charge_low:

            *result = fuel_b_threshold_low_charge();

            break;

        case __KV__batt_charge_critical:

            *result = fuel_b_threshold_critical_charge();

            break;

        #endif

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

        case __KV__linked:
            if( param_len != 1 ){

                break;
            }

            // param0 is a string
            ref.n = params[0];

            // dereference to pool:
            ptr = (int32_t *)( pools[ref.ref.pool] + ref.ref.addr );

            str = (char *)ptr;

            *result = link_b_is_linked( hash_u32_string( str ) );

            break;

        case __KV__clear:
            gfx_v_clear();
            break;

        case __KV__strlen:
            if( param_len != 1 ){

                break;
            }

            ref.n = params[0];

            // dereference to pool:
            ptr = (int32_t *)( pools[ref.ref.pool] + ref.ref.addr );

            str = (char *)ptr;

            *result = strlen( str );

            break;

        case __KV__strcmp:
            if( param_len != 2 ){

                break;
            }

            // dereference to pool:
            ref.n = params[0];
            ptr = (int32_t *)( pools[ref.ref.pool] + ref.ref.addr );
            str = (char *)ptr;

            ref.n = params[1];
            ptr = (int32_t *)( pools[ref.ref.pool] + ref.ref.addr );
            str2 = (char *)ptr;

            *result = strcmp( str, str2 ) == 0;

            break;

		default:
            // function not found
            return -1;
            break;
    }    
	
    return 0;
}
