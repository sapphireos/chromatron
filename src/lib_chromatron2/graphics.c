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

#include "system.h"
#include "util.h"
#include "logging.h"
#include "keyvalue.h"
#include "threading.h"
#include "timers.h"
#include "list.h"
#include "catbus_common.h"
#include "catbus.h"
#include "datetime.h"

#include "pixel.h"
#include "graphics.h"
#include "vm.h"
#include "timesync.h"
#include "kvdb.h"
#include "hash.h"


static uint16_t hs_fade = 1000;
static uint16_t v_fade = 1000;
static uint16_t gfx_master_dimmer = 65535;
static uint16_t gfx_sub_dimmer = 65535;
static uint16_t pix_count;
static uint16_t pix_size_x;
static uint16_t pix_size_y;
static bool gfx_interleave_x;
static bool gfx_transpose;
static uint16_t gfx_frame_rate = 100;
static uint8_t gfx_dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;

static uint16_t gfx_virtual_array_start;
static uint16_t gfx_virtual_array_length;

static bool pixel_transfer_enable = TRUE;

static volatile uint8_t run_flags;
#define FLAG_RUN_PARAMS         0x01
#define FLAG_RUN_VM_LOOP        0x02
#define FLAG_RUN_FADER          0x04

static uint16_t vm_timer_rate; 
static uint16_t vm0_frame_number;
static uint32_t last_vm0_frame_ts;
static int16_t frame_rate_adjust;
static uint8_t init_vm;

#define FADER_TIMER_RATE 625 // 20 ms (gfx timer)
#define PARAMS_TIMER_RATE 1000 // 1000 ms (system ms timer)

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );


static uint16_t calc_vm_timer( uint32_t ms ){

    return ( ms * 31250 ) / 1000;
}

static void update_vm_timer( void ){

    uint16_t new_timer = calc_vm_timer( gfx_frame_rate + frame_rate_adjust );

    if( new_timer != vm_timer_rate ){

        ATOMIC;
        vm_timer_rate = new_timer;

        // immediately update timer so we don't have to wait for 
        // current frame to complete.  this speeds up the response if
        // we're going from a really slow to a really fast rate.
        // GFX_TIMER.CCB = GFX_TIMER.CNT + vm_timer_rate;

        END_ATOMIC;
    }
}

static void param_error_check( void ){

    // error check
    if( pix_count > MAX_PIXELS ){

        pix_count = MAX_PIXELS;
    }
    else if( pix_count == 0 ){

        // this is necessary to prevent divide by 0 errors.
        // also, 0 pixels doesn't really make sense in a
        // pixel graphics library, does it?
        pix_count = 1;
    }

    if( ( (uint32_t)pix_size_x * (uint32_t)pix_size_y ) > pix_count ){

        pix_size_x = pix_count;
        pix_size_y = 1;
    }

    if( pix_size_x > pix_count ){

        pix_size_x = 1;
    }

    if( pix_size_y > pix_count ){

        pix_size_y = 1;
    }

    if( pix_count > 0 ){

        if( pix_size_x == 0 ){

            pix_size_x = 1;
        }

        if( pix_size_y == 0 ){

            pix_size_y = 1;
        }
    }

    if( gfx_frame_rate < 10 ){

        gfx_frame_rate = 10;
    }

    if( gfx_dimmer_curve < 8 ){

        gfx_dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;
    }
}


int8_t gfx_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        param_error_check();

        ATOMIC;
        run_flags |= FLAG_RUN_PARAMS;
        END_ATOMIC;

        update_vm_timer();
    }

    return 0;
}

KV_SECTION_META kv_meta_t gfx_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_count,                   gfx_i8_kv_handler,   "pix_count" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_sub_dimmer,              gfx_i8_kv_handler,   "gfx_sub_dimmer" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_master_dimmer,           gfx_i8_kv_handler,   "gfx_master_dimmer" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_size_x,                  gfx_i8_kv_handler,   "pix_size_x" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_size_y,                  gfx_i8_kv_handler,   "pix_size_y" },
    { SAPPHIRE_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &gfx_interleave_x,            gfx_i8_kv_handler,   "gfx_interleave_x" },
    { SAPPHIRE_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &gfx_transpose,               gfx_i8_kv_handler,   "gfx_transpose" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &hs_fade,                     gfx_i8_kv_handler,   "gfx_hsfade" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &v_fade,                      gfx_i8_kv_handler,   "gfx_vfade" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_frame_rate,              gfx_i8_kv_handler,   "gfx_frame_rate" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &gfx_dimmer_curve,            gfx_i8_kv_handler,   "gfx_dimmer_curve" },
    
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_virtual_array_start,     gfx_i8_kv_handler,   "gfx_varray_start" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_virtual_array_length,    gfx_i8_kv_handler,   "gfx_varray_length" },
};

void gfx_v_init( void ){

    if( pixel_u8_get_mode() == PIX_MODE_ANALOG ){

        // override size settings
        pix_count = 1;
        pix_size_x = 1;
        pix_size_y = 1;
    }

    param_error_check();

    gfxlib_v_init();

    pixel_v_init();


    thread_t_create( gfx_control_thread,
                PSTR("gfx_control"),
                0,
                0 );
}

bool gfx_b_running( void ){

    return pixel_transfer_enable;
}

uint16_t gfx_u16_get_frame_number( void ){

    return vm0_frame_number;
}

uint32_t gfx_u32_get_frame_ts( void ){

    return last_vm0_frame_ts;
}

void gfx_v_set_frame_number( uint16_t frame ){

    vm0_frame_number = frame;
}

void gfx_v_set_sync0( uint16_t frame, uint32_t ts ){

    int32_t frame_offset = (int32_t)vm0_frame_number - (int32_t)frame;
    int32_t time_offset = (int32_t)last_vm0_frame_ts - (int32_t)ts;

    int32_t corrected_time_offset = time_offset + ( frame_offset * gfx_frame_rate );

    // we are ahead
    if( corrected_time_offset > 10 ){

        // slow down
        frame_rate_adjust = 10;
    }
    // we are behind
    else if( corrected_time_offset < 10 ){

        // speed up
        frame_rate_adjust = -10;
    }

    log_v_debug_P( PSTR("offset net: %ld frame: %ld corr: %ld adj: %d"), time_offset, frame_offset, corrected_time_offset, frame_rate_adjust );

    update_vm_timer();
}


PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
//     static uint32_t last_param_sync;
//     static uint8_t flags;

//     // wait until wifi is attached before starting up pixel driver
//     THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

//     param_error_check();  


//     while(1){

//         THREAD_WAIT_WHILE( pt, ( run_flags == 0 ) && 
//                                ( tmr_u32_elapsed_time_ms( last_param_sync ) < PARAMS_TIMER_RATE ) );

//         ATOMIC;
//         flags = run_flags;
//         run_flags = 0;
//         END_ATOMIC;

//         if( flags & FLAG_RUN_VM_LOOP ){

//             // check if vm is running
//             if( !vm_b_running() ){

//                 goto end;
//             }

//             THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
//             send_run_vm_cmd();

//             if( vm_b_is_vm_running( 0 ) ){
                
//                 vm0_frame_number++;
//                 last_vm0_frame_ts = time_u32_get_network_time();
//             }
//         }

// end:
//         if( flags & FLAG_RUN_FADER ){   

//             if( pixel_u8_get_mode() != PIX_MODE_OFF ){
                
//                 THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
//                 send_run_fader_cmd();
//             }
//         }

//         if( ( flags & FLAG_RUN_PARAMS ) ||
//             ( tmr_u32_elapsed_time_ms( last_param_sync ) >= PARAMS_TIMER_RATE ) ){

//             THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
//             send_params( FALSE );

//             // run DB transfer
//             run_xfer = TRUE;
//             for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

//                 if( subscribed_keys[i].hash == 0 ){

//                    continue;
//                 }
                    
//                 subscribed_keys[i].flags |= KEY_FLAG_UPDATED;
//             }   

//             last_param_sync = tmr_u32_get_system_time_ms();
//         }

//         THREAD_YIELD( pt ); 
//     }

PT_END( pt );
}

void gfx_v_init_vm( uint8_t vm_id ){

    init_vm |= ( 1 << vm_id );

    if( vm_id == 0 ){

        vm0_frame_number = 0;
    }
}

