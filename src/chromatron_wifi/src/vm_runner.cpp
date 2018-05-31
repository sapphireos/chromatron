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

static uint16_t vm_load_len;

static uint8_t vm_data[VM_RUNNER_MAX_SIZE];
static int16_t vm_start[VM_MAX_VMS];
static uint16_t vm_size[VM_MAX_VMS];
static vm_state_t vm_state[VM_MAX_VMS];

static uint16_t vm_total_size;

static int8_t vm_status[VM_MAX_VMS];
static uint16_t vm_loop_time[VM_MAX_VMS];
static uint16_t vm_fader_time;
static uint16_t vm_thread_time[VM_MAX_VMS];
static uint16_t vm_max_cycles[VM_MAX_VMS];

static bool run_vm;
static bool run_faders;

static uint32_t thread_tick;

#define VM_RUN_INIT     0
#define VM_RUN_LOOP     1
#define VM_RUN_THREADS  2


uint32_t elapsed_time_millis( uint32_t start_time ){

    uint32_t end_time = millis();

    uint32_t elapsed;

    if( end_time >= start_time ){
        
        elapsed = end_time - start_time;
    }
    else{
        
        elapsed = UINT32_MAX - ( start_time - end_time );
    }

    if( elapsed > UINT16_MAX ){

        elapsed = UINT16_MAX;
    }

    return elapsed;
}

uint32_t elapsed_time_micros( uint32_t start_time ){

    uint32_t end_time = micros();

    uint32_t elapsed;

    if( end_time >= start_time ){
        
        elapsed = end_time - start_time;
    }
    else{
        
        elapsed = UINT32_MAX - ( start_time - end_time );
    }

    if( elapsed > UINT16_MAX ){

        elapsed = UINT16_MAX;
    }

    return elapsed;
}

static int8_t _vm_i8_run_vm( uint8_t mode, uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return -1;
    }

    // check that VM was loaded with no errors
    if( vm_status[vm_index] < 0 ){

        return -20;
    }

    // check that VM has not halted:
    if( vm_status[vm_index] == VM_STATUS_HALT ){

        return 0;
    }

    if( vm_start[vm_index] < 0 ){

        return -2;
    }

    uint32_t start_time = micros();

    int8_t return_code;

    uint8_t *stream = (uint8_t *)&vm_data[vm_start[vm_index]];

    if( mode == VM_RUN_INIT ){

        return_code = vm_i8_run_init( stream, &vm_state[vm_index] );
    }
    else if( mode == VM_RUN_LOOP ){

        return_code = vm_i8_run_loop( stream, &vm_state[vm_index] );
    }
    else{

        // check if there are any threads to run
        return_code = vm_i8_run_threads( stream, &vm_state[vm_index] );   

        // if no threads were run, bail out early so we don't 
        // transmit published vars that couldn't have changed.
        if( return_code == VM_STATUS_NO_THREADS ){

            return VM_STATUS_OK;
        }
    }

    // if return is anything other than OK, send status immediately
    if( return_code != 0 ){

        intf_v_request_vm_info();
    }

    if( return_code == VM_STATUS_HALT ){

        vm_status[vm_index] = VM_STATUS_HALT;
    }

    uint32_t elapsed = elapsed_time_micros( start_time );

    if( mode == VM_RUN_LOOP ){
    
        vm_loop_time[vm_index] = elapsed;
    }
    else if( mode == VM_RUN_THREADS ){
    
        vm_thread_time[vm_index] = elapsed;
    }

    vm_max_cycles[vm_index] = vm_state[vm_index].max_cycles;

    // queue published vars for transport
    vm_publish_t *publish = (vm_publish_t *)&stream[vm_state[vm_index].publish_start];

    uint32_t count = vm_state[vm_index].publish_count;

    while( count > 0 ){

        intf_v_send_kv( publish->hash );

        publish++;
        count--;
    }

    // load write keys from DB for transport
    count = vm_state[vm_index].write_keys_count;
    uint32_t *hash = (uint32_t *)&stream[vm_state[vm_index].write_keys_start];

    while( count > 0 ){

        intf_v_send_kv( *hash );

        hash++;
        count--;
    }

    return return_code;
}


void vm_v_init( void ){

    for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

        vm_start[i] = -1;
        vm_status[i] = VM_STATUS_NOT_RUNNING;
        vm_v_reset( i );
    }

    // clear VM data
    memset( vm_data, 0xff, sizeof(vm_data) );

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

        for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

            _vm_i8_run_vm( VM_RUN_LOOP, i ); 
        }

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

        vm_fader_time = elapsed;

        run_faders = false;
    }

    if( elapsed_time_millis( thread_tick ) >= VM_RUNNER_THREAD_RATE ){

        thread_tick = millis();

        for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

            _vm_i8_run_vm( VM_RUN_THREADS, i );
        }
    }
}

void vm_v_reset( uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return;
    }

    // check if this VM has already been reset
    if( vm_start[vm_index] < 0 ){

        return;
    }


    uint8_t *stream = (uint8_t *)&vm_data[vm_start[vm_index]];

    // write 1s to VM data (trap instruction)
    memset( stream, 0xff, vm_size[vm_index] );

 
    int32_t dirty_start = vm_start[vm_index];
    int32_t clean_start = vm_start[vm_index] + vm_size[vm_index];

    // defrag VMs
    bool moved = false;

    do{
        for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

            // looking for VM at the start of the clean section
            if( vm_start[i] == clean_start ){

                // copy this VM into the area we just erased

                // must use memmove here, since dest and src might overlap!
                memmove( &vm_data[dirty_start], &vm_data[vm_start[i]], vm_size[i] );
                vm_start[i] = dirty_start;

                clean_start += vm_size[i];
                dirty_start += vm_size[i];

                moved = true;
            }
        }
    } while( moved );

    
    vm_total_size -= vm_size[vm_index];

    vm_start[vm_index] = -1;
    vm_size[vm_index] = 0;

    memset( &vm_state[vm_index], 0, sizeof(vm_state[vm_index]) );

    // don't reset status if there is an error
    if( vm_status[vm_index] >= 0 ){
        
        vm_status[vm_index] = VM_STATUS_NOT_RUNNING;
    }

    vm_load_len = 0;

    vm_v_clear_db( KVDB_VM_RUNNER_TAG + vm_index );
}

int8_t vm_i8_load( uint8_t *data, uint16_t len, uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return -1;
    }

    // bounds check VM
    if( ( len + vm_total_size ) >= sizeof(vm_data) ){
        
        return -2;
    }

    // reset status codes
    vm_status[vm_index] = VM_STATUS_NOT_RUNNING;

    int8_t status = 0;

    // check if this is the first page
    if( vm_start[vm_index] == -1 ){

        // need to get starting offset
        vm_start[vm_index] = vm_total_size;

        // make sure we start our VM size as 0
        vm_size[vm_index] = 0;
    }

    uint8_t *stream = (uint8_t *)&vm_data[vm_start[vm_index]];
    
    if( len > 0 ){
    
        // load next page of data
        memcpy( &stream[vm_size[vm_index]], data, len );

        vm_size[vm_index] += len;
        vm_total_size += len;
    }
    // length of 0 indicates loading is finished
    else{

        status = vm_i8_load_program( 0, stream, vm_size[vm_index], &vm_state[vm_index] );

        vm_state[vm_index].prog_size = vm_size[vm_index];
        vm_state[vm_index].tick_rate = VM_RUNNER_THREAD_RATE;

        uint8_t *code_start = (uint8_t *)( stream + vm_state[vm_index].code_start );
        int32_t *data_start = (int32_t *)( stream + vm_state[vm_index].data_start );

        // check that code pointer starts on 32 bit boundary
        if( ( (uint32_t)code_start & 0x03 ) != 0 ){

            status = VM_STATUS_CODE_MISALIGN;
        }
        // check that data pointer starts on 32 bit boundary
        else if( ( (uint32_t)data_start & 0x03 ) != 0 ){

            status = VM_STATUS_DATA_MISALIGN;
        }

        vm_status[vm_index] = status;

        if( status < 0 ){

            goto end;
        }

        // init RNG seed to device ID
        uint8_t mac[6];
        intf_v_get_mac( mac );
        uint64_t rng_seed = ( (uint64_t)mac[5] << 40 ) + 
                            ( (uint64_t)mac[1] << 32 ) +
                            ( (uint64_t)mac[2] << 24 ) + 
                            ( (uint64_t)mac[3] << 16 ) + 
                            ( (uint64_t)mac[4] << 8 ) + 
                            mac[5];
        // make sure seed is never 0 (otherwise RNG will not work)
        if( rng_seed == 0 ){

            rng_seed = 1;
        }

        vm_state[vm_index].rng_seed = rng_seed;

        // init database
        vm_v_init_db( stream, &vm_state[vm_index], KVDB_VM_RUNNER_TAG + vm_index );
        
        // run init function
        status = _vm_i8_run_vm( VM_RUN_INIT, vm_index );
    }

end:

    if( status < 0 ){

        vm_v_reset( vm_index );
    }

    return status;
}

int32_t vm_i32_get_reg( uint8_t addr, uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return 0;
    }

    if( vm_status[vm_index] < 0 ){

        return 0;
    }

    if( vm_start[vm_index] < 0 ){

        return 0;
    }

    uint8_t *stream = (uint8_t *)&vm_data[vm_start[vm_index]];

    return vm_i32_get_data( stream, &vm_state[vm_index], addr );
}

void vm_v_set_reg( uint8_t addr, int32_t data, uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return;
    }

    if( vm_status[vm_index] < 0 ){

        return;
    }

    if( vm_start[vm_index] < 0 ){

        return;
    }

    uint8_t *stream = (uint8_t *)&vm_data[vm_start[vm_index]];

    vm_v_set_data( stream, &vm_state[vm_index], addr, data );
}

void vm_v_get_info( uint8_t index, vm_info_t *info ){

    if( index >= VM_MAX_VMS ){

        return;
    }

    info->status        = vm_status[index];
    info->loop_time     = vm_loop_time[index];
    info->thread_time   = vm_thread_time[index];
    info->max_cycles    = vm_max_cycles[index];
}

uint16_t vm_u16_get_fader_time( void ){

    return vm_fader_time;
}

uint16_t vm_u16_get_total_size( void ){

    return vm_total_size;
}

int8_t vm_i8_get_frame_sync( uint8_t index, wifi_msg_vm_frame_sync_t *sync ){

    if( index > 0 ){

        return -1;
    }

    uint8_t vm_index = 0;

    sync->frame_number  = vm_state[vm_index].frame_number;
    sync->rng_seed      = vm_state[vm_index].rng_seed;

    // for now, we only send one chunk of register data
    sync->data_index    = 0;
    sync->data_count    = vm_state[vm_index].data_count;

    if( sync->data_count > WIFI_DATA_FRAME_SYNC_MAX_DATA ){

        sync->data_count = WIFI_DATA_FRAME_SYNC_MAX_DATA;
    }

    if( vm_start[vm_index] < 0 ){

        return 0;
    }

    uint8_t *stream = (uint8_t *)&vm_data[vm_start[vm_index]];

    vm_v_get_data_multi( stream, &vm_state[vm_index], sync->data_index, sync->data_count, sync->data );

    return 0;
}


uint8_t vm_u8_set_frame_sync( wifi_msg_vm_frame_sync_t *sync ){

    uint8_t status = 0;

    int32_t frame_diff = (int32_t)vm_state[0].frame_number - (int32_t)sync->frame_number;

    if( ( frame_diff > 1 ) || ( frame_diff < -1 ) ){

        status |= 0x80;
    }

    if( vm_state[0].frame_number > sync->frame_number ){

        status |= 0x40;

        vm_state[0].frame_number   = sync->frame_number;
    }
    else if( vm_state[0].frame_number < sync->frame_number ){

        status |= 0x20;

        vm_state[0].frame_number   = sync->frame_number;
    }

    if( vm_state[0].rng_seed != sync->rng_seed ){

        vm_state[0].rng_seed       = sync->rng_seed;
        status |= 0x01;   
    }

    for( uint8_t i = 0; i < sync->data_count; i++ ){

        if( vm_i32_get_reg( i + sync->data_index, 0 ) != sync->data[i] ){

            vm_v_set_reg( i + sync->data_index, sync->data[i], 0 );
            status |= 0x02;
        }
    }   

    return status;
}

uint16_t vm_u16_get_frame_number( void ){

    return vm_state[0].frame_number;
}

