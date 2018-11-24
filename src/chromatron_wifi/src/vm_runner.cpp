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
#include "comm_printf.h"

extern "C"{
    #include "vm_runner.h"
    #include "vm_core.h"
    #include "gfx_lib.h"
    #include "list.h"
    #include "wifi_cmd.h"
    #include "kvdb.h"
    #include "vm_wifi_cmd.h"
    #include "catbus_common.h"
    #include "hash.h"
    #include "datetime_struct.h"
    #include "memory.h"
}

// static uint8_t vm_data[VM_RUNNER_MAX_SIZE];
// static int16_t vm_start[VM_MAX_VMS];
// static uint16_t vm_size[VM_MAX_VMS];

static mem_handle_t vm_handles[VM_MAX_VMS];
static vm_state_t vm_state[VM_MAX_VMS];

static uint16_t vm_total_size;

static int8_t vm_status[VM_MAX_VMS];
static uint16_t vm_loop_time[VM_MAX_VMS];
static uint16_t vm_fader_time;
static uint16_t vm_thread_time[VM_MAX_VMS];

static bool run_vm;
static bool run_faders;
static bool run_cron;

static uint32_t cron_seconds;

static uint32_t thread_tick;

#define VM_RUN_INIT     0
#define VM_RUN_LOOP     1
#define VM_RUN_THREADS  2
#define VM_RUN_CRON     3

static catbus_hash_t32 kv_hashes[32];
static uint8_t kv_index;

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

void vm_v_send_info( void ){

    wifi_msg_vm_info_t msg;

    for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

        vm_v_get_info( i, &msg.vm_info[i] );
    }

    msg.fader_time      = vm_u16_get_fader_time();
    msg.vm_total_size   = vm_u16_get_total_size();

    intf_i8_send_msg( WIFI_DATA_ID_VM_INFO, (uint8_t *)&msg, sizeof(msg) );
}

static int8_t _vm_i8_run_vm( uint8_t mode, uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return -1;
    }

    // check that VM was loaded with no errors
    if( vm_status[vm_index] < 0 ){

        return -20;
    }

    // check that VM has not halted or waiting for sync
    if( ( vm_status[vm_index] == VM_STATUS_HALT ) ||
        ( vm_status[vm_index] == VM_STATUS_WAIT_SYNC ) ){

        return 0;
    }

    if( vm_handles[vm_index] <= 0 ){

        return -2;
    }

    // check that VM is READY, but we're not calling init
    if( ( vm_status[vm_index] == VM_STATUS_READY ) && ( mode != VM_RUN_INIT ) ){

        // need to run init before we do anything else

        return 0;
    }

    uint32_t start_time = micros();

    int8_t return_code = VM_STATUS_ERROR;

    uint8_t *stream = (uint8_t *)mem2_vp_get_ptr( vm_handles[vm_index] );

    if( mode == VM_RUN_INIT ){

        return_code = vm_i8_run_init( stream, &vm_state[vm_index] );
    }
    else if( mode == VM_RUN_LOOP ){

        return_code = vm_i8_run_loop( stream, &vm_state[vm_index] );
    }
    else if( mode == VM_RUN_THREADS ){

        // check if there are any threads to run
        return_code = vm_i8_run_threads( stream, &vm_state[vm_index] );   

        // if no threads were run, bail out early so we don't 
        // transmit published vars that couldn't have changed.
        if( return_code == VM_STATUS_NO_THREADS ){

            return VM_STATUS_OK;
        }
    }
    else if( mode == VM_RUN_CRON ){

        return_code = vm_i8_run_cron( stream, &vm_state[vm_index], cron_seconds ); 
    }
    else{

        // not running anything.
        return VM_STATUS_OK;
    }

    // if return is anything other than OK, send status immediately
    if( return_code != 0 ){

        vm_v_send_info();
    }

    if( return_code == VM_STATUS_HALT ){

        vm_status[vm_index] = VM_STATUS_HALT;
    }
    else if( return_code < 0 ){

        vm_status[vm_index] = return_code;   
    }

    uint32_t elapsed = elapsed_time_micros( start_time );

    if( mode == VM_RUN_LOOP ){
    
        vm_loop_time[vm_index] = elapsed;
    }
    else if( mode == VM_RUN_THREADS ){
    
        vm_thread_time[vm_index] = elapsed;
    }

    return return_code;
}


void vm_v_init( void ){

    for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

        vm_handles[i] = -1;
        vm_status[i] = VM_STATUS_NOT_RUNNING;
        vm_v_reset( i );
    }

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

    // check cron jobs
    if( run_cron ){

        for( uint32_t i = 0; i < VM_MAX_VMS; i++ ){

            _vm_i8_run_vm( VM_RUN_CRON, i );
        }

        run_cron = false;
    }

    // get all updated KVDB items and transmit them

    uint8_t buf[CATBUS_MAX_DATA + sizeof(wifi_msg_kv_data_t)];
    wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)buf;
    uint8_t *data = (uint8_t *)( msg + 1 );

    msg->tag = 0;

    for( uint32_t i = 0; i < kv_index; i++ ){

        if( kvdb_i8_get_meta( kv_hashes[i], &msg->meta ) < 0 ){

            continue;
        }

        uint16_t data_len = type_u16_size( msg->meta.type ) * ( (uint16_t)msg->meta.count + 1 );

        kvdb_i8_get( kv_hashes[i], msg->meta.type, data, CATBUS_MAX_DATA );        

        intf_i8_send_msg( WIFI_DATA_ID_KV_DATA, buf, data_len + sizeof(wifi_msg_kv_data_t) );
    }

    kv_index = 0;
}

void vm_v_reset( uint8_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return;
    }

    // check if this VM has already been reset
    if( vm_handles[vm_index] < 0 ){

        return;
    }

    vm_v_clear_db( 1 << vm_index );

    vm_total_size -= mem2_u16_get_size( vm_handles[vm_index] );

    memset( &vm_state[vm_index], 0, sizeof(vm_state[vm_index]) );

    // don't reset status if there is an error
    if( vm_status[vm_index] >= 0 ){
        
        vm_status[vm_index] = VM_STATUS_NOT_RUNNING;
    }

    vm_loop_time[vm_index] = 0;
    vm_thread_time[vm_index] = 0;

    mem2_v_free( vm_handles[vm_index] );
    vm_handles[vm_index] = -1;
}

int8_t vm_i8_load( uint8_t *data, uint16_t len, uint16_t total_size, uint16_t offset, uint8_t vm_index ){

    int8_t status = 0;
    uint8_t *stream = 0;

    if( vm_index >= VM_MAX_VMS ){

        return -1;
    }

    // check offset for beginning of file
    if( offset == 0 ){

        // bounds check VM
        if( ( total_size + vm_total_size ) >= VM_RUNNER_MAX_SIZE ){
            
            return -2;
        }

        // check if this slot is already loaded
        if( vm_handles[vm_index] > 0 ){

            mem2_v_free( vm_handles[vm_index] );
            vm_handles[vm_index] = -1;
        }

        // allocate memory
        vm_handles[vm_index] = mem2_h_alloc( total_size );
    }
    
    // verify handle is allocated
    if( vm_handles[vm_index] <= 0 ){

        return -3;
    }

    // verify offset and length are within bounds
    if( ( offset + len ) > mem2_u16_get_size( vm_handles[vm_index] ) ){

        goto end;
    }

    // reset status codes
    vm_status[vm_index] = VM_STATUS_NOT_RUNNING;

    stream = (uint8_t *)mem2_vp_get_ptr( vm_handles[vm_index] );
    
    if( len > 0 ){
    
        // load next page of data
        memcpy( &stream[offset], data, len );

        vm_total_size += len;
    }
    // length of 0 indicates loading is finished
    else{

        status = vm_i8_load_program( 0, stream, mem2_u16_get_size( vm_handles[vm_index] ), &vm_state[vm_index] );

        vm_state[vm_index].prog_size = mem2_u16_get_size( vm_handles[vm_index] );
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
        vm_v_init_db( stream, &vm_state[vm_index], 1 << vm_index );


        // VM is ready for init
        vm_status[vm_index] = VM_STATUS_READY;
    }

end:

    if( status < 0 ){

        vm_v_reset( vm_index );
    }

    return status;
}

int8_t vm_i8_start( uint32_t vm_index ){

    if( vm_index >= VM_MAX_VMS ){

        return 0;
    }

    if( vm_status[vm_index] != VM_STATUS_READY ){

        return -1;
    }

    vm_status[vm_index] = VM_STATUS_OK;

    int8_t status = _vm_i8_run_vm( VM_RUN_INIT, vm_index );

    if( status < 0 ){

        vm_v_reset( vm_index );
    }

    return status;
}

void vm_v_get_info( uint8_t index, vm_info_t *info ){

    if( index >= VM_MAX_VMS ){

        return;
    }

    info->status        = vm_status[index];
    info->loop_time     = vm_loop_time[index];
    info->thread_time   = vm_thread_time[index];
    info->max_cycles    = vm_state[index].max_cycles;
}

uint16_t vm_u16_get_fader_time( void ){

    return vm_fader_time;
}

uint16_t vm_u16_get_total_size( void ){

    return vm_total_size;
}

void vm_v_start_frame_sync( uint8_t index, wifi_msg_vm_frame_sync_t *msg, uint16_t len ){

    if( index >= VM_MAX_VMS ){

        return;
    }

    intf_v_printf( "sync: %u", msg->data_len );

    // check that the VM is running normally
    if( vm_status[index] != VM_STATUS_OK ){

        // can't sync at this time

        return;
    }

    // verify data length and program hash match
    if( ( vm_state[index].program_name_hash != msg->program_name_hash ) ||
        ( vm_state[index].data_len != msg->data_len ) ){

        intf_v_printf( "sync param error" );

        vm_status[index] = VM_STATUS_SYNC_FAIL;

        return;
    }

    // set state
    vm_status[index] = VM_STATUS_WAIT_SYNC;

    vm_state[index].rng_seed        = msg->rng_seed;
    vm_state[index].frame_number    = msg->frame_number;
}

void vm_v_frame_sync_data( uint8_t index, wifi_msg_vm_sync_data_t *msg, uint16_t len ){

    if( index >= VM_MAX_VMS ){

        return;
    }

    intf_v_printf( "offset: %u", msg->offset );

    uint16_t data_len = len - sizeof(wifi_msg_vm_sync_data_t);

    if( ( msg->offset + data_len ) > vm_state[index].data_len ){

        intf_v_printf( "sync overflow: %u", len );

        vm_status[index] = VM_STATUS_SYNC_FAIL;

        return;
    }

    uint8_t *src = (uint8_t *)( msg + 1 );

    uint8_t *stream = (uint8_t *)mem2_vp_get_ptr( vm_handles[index] );
    uint8_t *data = stream + vm_state[index].data_start;

    memcpy( &data[msg->offset], src, data_len );
}

void vm_v_frame_sync_done( uint8_t index, wifi_msg_vm_sync_done_t *msg, uint16_t len ){

    if( index >= VM_MAX_VMS ){

        return;
    }

    intf_v_printf( "done: %lx", msg->hash );

    // check hash
    uint8_t *stream = (uint8_t *)mem2_vp_get_ptr( vm_handles[index] );
    uint8_t *data = stream + vm_state[index].data_start;

    uint32_t hash = hash_u32_data( data, vm_state[index].data_len );

    if( hash == msg->hash ){

        intf_v_printf( "verified" );

        // set state
        vm_status[index] = VM_STATUS_OK;
    }
    else{

        vm_status[index] = VM_STATUS_SYNC_FAIL;

        intf_v_printf( "%lx != %lx", hash, msg->hash );
    }
}


void vm_v_set_time_of_day( wifi_msg_vm_time_of_day_t *msg ){

    cron_seconds = msg->seconds;
    
    run_cron = true;
}

void vm_v_request_frame_data( uint8_t index ){

    if( index >= VM_MAX_VMS ){

        return;
    }

    uint8_t buf[WIFI_MAX_SYNC_DATA + sizeof(wifi_msg_vm_frame_sync_t)];
    
    wifi_msg_vm_frame_sync_t msg;
    msg.rng_seed            = vm_state[index].rng_seed;
    msg.frame_number        = vm_state[index].frame_number;
    msg.data_len            = vm_state[index].data_len;
    msg.program_name_hash   = vm_state[index].program_name_hash;

    intf_i8_send_msg( WIFI_DATA_ID_VM_FRAME_SYNC, (uint8_t *)&msg, sizeof(msg) );

    uint32_t hash = hash_u32_start();

    uint16_t len = vm_state[index].data_len;

    uint8_t *stream = (uint8_t *)mem2_vp_get_ptr( vm_handles[index] );
    uint8_t *src = stream + vm_state[index].data_start;

    wifi_msg_vm_sync_data_t *sync = (wifi_msg_vm_sync_data_t *)buf;
    uint8_t *dst = (uint8_t *)( sync + 1 );

    if( ( (uint32_t)dst & 0x03 ) != 0 ){

        intf_v_printf("sync dst alignment error");
        return;
    }

    if( ( (uint32_t)src & 0x03 ) != 0 ){

        intf_v_printf("sync src alignment error");
        return;
    }

    sync->offset = 0;
    sync->padding = 0;

    while( len > 0 ){

        uint32_t copy_len = len;
        if( copy_len > WIFI_MAX_SYNC_DATA){

            copy_len = WIFI_MAX_SYNC_DATA;
        }

        memcpy( dst, src, copy_len );
        hash = hash_u32_partial( hash, dst, copy_len );

        intf_i8_send_msg( WIFI_DATA_ID_VM_SYNC_DATA, buf, sizeof(wifi_msg_vm_sync_data_t) + copy_len );

        sync->offset =+ copy_len;

        src += copy_len;
        len -= copy_len;
    }

    wifi_msg_vm_sync_done_t done;
    done.hash = hash;

    intf_i8_send_msg( WIFI_DATA_ID_VM_SYNC_DONE, (uint8_t *)&done, sizeof(done) );
}

void kvdb_v_notify_set( catbus_hash_t32 hash, catbus_meta_t *meta, const void *data ){

    if( kv_index >= cnt_of_array(kv_hashes) ){

        return;
    }

    // check if hash is already in the list
    for( uint32_t i = 0; i < kv_index; i++ ){

        if( kv_hashes[i] == hash ){

            return;
        }
    }

    kv_hashes[kv_index] = hash;    
    kv_index++;
}








