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

#include <Arduino.h>
#include "comm_intf.h"

extern "C"{
    #include "vm_runner.h"
    #include "vm_core.h"
    #include "gfx_lib.h"
    #include "list.h"
    #include "wifi_cmd.h"
    #include "kvdb.h"
}

static uint16_t vm_offset;
static uint16_t vm_len;
static uint8_t vm_slab[4096];
static vm_state_t vm_state;

static vm_info_t vm_info;

static uint32_t fader_time_start;
static uint32_t vm_time_start;
static uint32_t last_frame_sync;

static bool run_vm;
static bool run_faders;


static int8_t _vm_i8_run_vm( bool init ){

    // check that VM was loaded with no errors
    if( vm_info.status < 0 ){

        return -20;
    }

    // check that VM has not halted:
    if( vm_info.return_code == VM_STATUS_HALT ){

        return 0;
    }

    int32_t *data_table = (int32_t *)&vm_slab[vm_state.data_start];

    // init pixel array pointer
    gfx_pixel_array_t *pix_array = (gfx_pixel_array_t *)( vm_slab + vm_state.pix_obj_start );

    // load published vars
    vm_publish_t *publish = (vm_publish_t *)&vm_slab[vm_state.publish_start];

    uint32_t count = vm_state.publish_count;

    while( count > 0 ){

        kvdb_i8_get( publish->hash, CATBUS_TYPE_INT32, &data_table[publish->addr], sizeof(data_table[publish->addr]) );

        publish++;
        count--;
    }

    uint32_t start_time = micros();

    int8_t return_code;

    if( init ){

        // gfx_v_reset();
        gfx_v_init_pixel_arrays( pix_array, vm_state.pix_obj_count );

        return_code = vm_i8_run_init( vm_slab, &vm_state );
    }
    else{

        return_code = vm_i8_run_loop( vm_slab, &vm_state );
    }

    // if return is anything other than OK, send status immediately
    if( return_code != 0 ){

        intf_v_request_vm_info();
    }

    uint32_t end_time = micros();
    uint32_t elapsed;

    if( end_time >= start_time ){
        
        elapsed = end_time - start_time;
    }
    else{
        
        elapsed = UINT32_MAX - ( start_time - end_time );
    }

    vm_info.loop_time = elapsed;
    vm_info.max_cycles = vm_state.max_cycles;
    vm_info.return_code = return_code;

    // store published vars back to DB, also queue for transport
    publish = (vm_publish_t *)&vm_slab[vm_state.publish_start];

    count = vm_state.publish_count;

    while( count > 0 ){

        kvdb_i8_set( publish->hash, CATBUS_TYPE_INT32, &data_table[publish->addr], sizeof(data_table[publish->addr]) );

        intf_v_send_kv( publish->hash );

        publish++;
        count--;
    }

    // load write keys from DB
    count = vm_state.write_keys_count;
    uint32_t *hash = (uint32_t *)&vm_slab[vm_state.write_keys_start];

    while( count > 0 ){

        intf_v_send_kv( *hash );

        hash++;
        count--;
    }


end:
    return return_code;
}


void vm_v_init( void ){

    vm_v_reset();

    fader_time_start = millis();

    gfxlib_v_init();
    gfx_v_reset();
}

void vm_v_run_faders( void ){

    run_faders = true;
}

void vm_v_run_vm( void ){

    run_vm = true;
}

void vm_v_process( void ){

    if( run_vm ){

        vm_v_run_loop();
        run_vm = false;
    }

    if( run_faders ){

        uint32_t start = micros();

        gfx_v_process_faders();
        gfx_v_sync_array();

        #ifdef USE_HSV_BRIDGE
        intf_v_request_hsv_array();
        #else
        intf_v_request_rgb_pix0();
        intf_v_request_rgb_array();
        #endif

        // TODO this will have rollover issues
        uint32_t elapsed = micros() - start;

        vm_info.fader_time = elapsed;

        run_faders = false;
    }
}

void vm_v_reset( void ){

    vm_info.status = -127;
    vm_info.return_code = -127;

    vm_offset = 0;
    vm_len = 0;
    memset( vm_slab, 0, sizeof(vm_slab) );

}

int8_t vm_i8_load( uint8_t *data, uint16_t len ){

    // reset status codes
    vm_info.status = -127;
    vm_info.return_code = -127;

    int8_t status = 0;

    if( ( len + vm_offset ) > sizeof(vm_slab) ){

        vm_info.status = VM_STATUS_IMAGE_TOO_LARGE;
        return -1;
    }

    memcpy( &vm_slab[vm_offset], data, len );
    vm_offset += len;
    vm_len += len;

    // length of 0 indicates loading is finished
    if( len == 0 ){

        vm_state.prog_size = vm_len;

        status = vm_i8_load_program( 0, vm_slab, vm_len, &vm_state );

        uint8_t *ptr = vm_slab;
        uint8_t *code_start = (uint8_t *)( ptr + vm_state.code_start );
        int32_t *data_start = (int32_t *)( ptr + vm_state.data_start );

        // check that code pointer starts on 32 bit boundary
        if( ( (uint32_t)code_start & 0x03 ) != 0 ){

            status = VM_STATUS_CODE_MISALIGN;
        }

        // check that data pointer starts on 32 bit boundary
        if( ( (uint32_t)data_start & 0x03 ) != 0 ){

            status = VM_STATUS_DATA_MISALIGN;
        }

        vm_info.status = status;

        if( status < 0 ){

            return status;
        }

        // init RNG seed to device ID
        uint8_t mac[6];
        intf_v_get_mac( mac );
        uint64_t rng_seed = ( (uint64_t)mac[5] << 40 ) + ( (uint64_t)mac[1] << 32 ) + ( (uint64_t)mac[2] << 24 ) + ( (uint64_t)mac[3] << 16 ) + ( (uint64_t)mac[4] << 8 ) + mac[5];
        // make sure seed is never 0 (otherwise RNG will not work)
        if( rng_seed == 0 ){

            rng_seed = 1;
        }

        vm_state.rng_seed = rng_seed;

        // init database
        kvdb_v_delete_tag( KVDB_VM_RUNNER_TAG );

        // load published vars to DB
        uint32_t count = vm_state.publish_count;
        vm_publish_t *publish = (vm_publish_t *)&vm_slab[vm_state.publish_start];
    
        while( count > 0 ){        

            kvdb_i8_add( publish->hash, CATBUS_TYPE_INT32, 1, 0, 0 );
            kvdb_v_set_tag( publish->hash, KVDB_VM_RUNNER_TAG );

            publish++;
            count--;
        }

        status = _vm_i8_run_vm( true );
    }

    return status;
}

void vm_v_run_loop( void ){

    int8_t status = _vm_i8_run_vm( false );    
}

int32_t vm_i32_get_reg( uint8_t addr ){

    if( vm_info.status < 0 ){

        return 0;
    }

    return vm_i32_get_data( vm_slab, &vm_state, addr );
}

void vm_v_set_reg( uint8_t addr, int32_t data ){

    if( vm_info.status < 0 ){

        return;
    }

    vm_v_set_data( vm_slab, &vm_state, addr, data );
}

void vm_v_get_info( vm_info_t *info ){

    *info = vm_info;
}

int8_t vm_i8_get_frame_sync( uint8_t index, wifi_msg_vm_frame_sync_t *sync ){

    if( index > 0 ){

        return -1;
    }

    sync->frame_number  = vm_state.frame_number;
    sync->rng_seed      = vm_state.rng_seed;

    // for now, we only send one chunk of register data
    sync->data_index    = 0;
    sync->data_count    = vm_state.data_count;

    if( sync->data_count > WIFI_DATA_FRAME_SYNC_MAX_DATA ){

        sync->data_count = WIFI_DATA_FRAME_SYNC_MAX_DATA;
    }

    vm_v_get_data_multi( vm_slab, &vm_state, sync->data_index, sync->data_count, sync->data );

    return 0;
}


uint8_t vm_u8_set_frame_sync( wifi_msg_vm_frame_sync_t *sync ){

    uint8_t status = 0;

    int32_t frame_diff = (int32_t)vm_state.frame_number - (int32_t)sync->frame_number;

    if( ( frame_diff > 1 ) || ( frame_diff < -1 ) ){

        status |= 0x80;
    }

    if( vm_state.frame_number > sync->frame_number ){

        status |= 0x40;

        vm_state.frame_number   = sync->frame_number;
    }
    else if( vm_state.frame_number < sync->frame_number ){

        status |= 0x20;

        vm_state.frame_number   = sync->frame_number;
    }

    if( vm_state.rng_seed != sync->rng_seed ){

        vm_state.rng_seed       = sync->rng_seed;
        status |= 0x01;   
    }

    for( uint8_t i = 0; i < sync->data_count; i++ ){

        if( vm_i32_get_reg( i + sync->data_index ) != sync->data[i] ){

            vm_v_set_reg( i + sync->data_index, sync->data[i] );
            status |= 0x02;
        }
    }   

    return status;
}

uint16_t vm_u16_get_frame_number( void ){

    return vm_state.frame_number;
}

