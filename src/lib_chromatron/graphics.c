// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "esp8266.h"
#include "vm.h"
#include "timesync.h"
#include "kvdb.h"
#include "hash.h"
#include "vm_wifi_cmd.h"
#include "vm_sync.h"



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
static uint16_t old_frame_rate = 100;
static uint8_t gfx_dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;
static uint8_t gfx_sat_curve = GFX_SAT_CURVE_DEFAULT;

static uint16_t gfx_virtual_array_start;
static uint16_t gfx_virtual_array_length;

static bool pixel_transfer_enable = TRUE;

// static volatile uint8_t run_flags;
// #define FLAG_RUN_VM_LOOP        0x02
static bool update_frame_rate;

// static uint16_t vm_timer_rate; 
static uint16_t vm0_frame_number;
static uint32_t vm0_frame_ts;
// static uint16_t vm0_sync_frame_number;
// static uint32_t vm0_sync_frame_ts;
static int16_t frame_rate_adjust;

// #define FADER_TIMER_RATE 625 // 20 ms (gfx timer)

PT_THREAD( gfx_fader_thread( pt_t *pt, void *state ) );
PT_THREAD( gfx_vm_loop_thread( pt_t *pt, void *state ) );

typedef struct{
    catbus_hash_t32 hash;
    uint8_t tag;
    uint8_t flags;
} subscribed_key_t;
#define KEY_FLAG_UPDATED        0x01

static subscribed_key_t subscribed_keys[32];

static uint16_t vm_fader_time;


// static uint16_t calc_vm_timer( uint32_t ms ){

//     return ( ms * 31250 ) / 1000;
// }

static void update_vm_timer( void ){

    update_frame_rate = TRUE;

    // uint16_t new_timer = calc_vm_timer( gfx_frame_rate + frame_rate_adjust );

    // if( new_timer != vm_timer_rate ){

    //     ATOMIC;
    //     vm_timer_rate = new_timer;

    //     // immediately update timer so we don't have to wait for 
    //     // current frame to complete.  this speeds up the response if
    //     // we're going from a really slow to a really fast rate.
    //     GFX_TIMER.CCB = GFX_TIMER.CNT + vm_timer_rate;

    //     END_ATOMIC;
    // }
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

    if( gfx_sat_curve < 8 ){

        gfx_sat_curve = GFX_SAT_CURVE_DEFAULT;
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

        gfx_v_sync_params();

        if( hash == __KV__gfx_frame_rate ){

            if( gfx_frame_rate != old_frame_rate ){

                old_frame_rate = gfx_frame_rate;
                update_vm_timer();    
            }
        }
        
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
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &gfx_sat_curve,               gfx_i8_kv_handler,   "gfx_sat_curve" },
    
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_virtual_array_start,     gfx_i8_kv_handler,   "gfx_varray_start" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_virtual_array_length,    gfx_i8_kv_handler,   "gfx_varray_length" },

    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY,  &vm_fader_time,            0,                   "vm_fade_time" },
};

// void gfx_v_set_params( gfx_params_t *params ){

//     if( params->version != GFX_VERSION ){

//         return;
//     }

//     pix_count                   = params->pix_count;
//     pix_size_x                  = params->pix_size_x;
//     pix_size_y                  = params->pix_size_y;
//     gfx_interleave_x            = params->interleave_x;
//     gfx_transpose               = params->transpose;
//     hs_fade                     = params->hs_fade;
//     v_fade                      = params->v_fade;
//     gfx_master_dimmer           = params->master_dimmer;
//     gfx_sub_dimmer              = params->sub_dimmer;
//     gfx_frame_rate              = params->frame_rate;
//     gfx_dimmer_curve            = params->dimmer_curve;
//     gfx_virtual_array_start     = params->virtual_array_start;
//     gfx_virtual_array_length    = params->virtual_array_length;

//     // we cannot set pix mode via this function
//     // pix_mode                = params->pix_mode;


//     param_error_check();

//     update_vm_timer();
// }

void gfx_v_get_params( gfx_params_t *params ){

    params->version             = GFX_VERSION;
    params->pix_count           = pix_count;
    params->pix_size_x          = pix_size_x;
    params->pix_size_y          = pix_size_y;
    params->interleave_x        = gfx_interleave_x;
    params->transpose           = gfx_transpose;
    params->hs_fade             = hs_fade;
    params->v_fade              = v_fade;
    params->master_dimmer       = gfx_master_dimmer;
    params->sub_dimmer          = gfx_sub_dimmer;
    params->frame_rate          = gfx_frame_rate;   
    params->dimmer_curve        = gfx_dimmer_curve;
    params->sat_curve           = gfx_sat_curve;
    params->pix_mode            = pixel_u8_get_mode();

    params->virtual_array_start   = gfx_virtual_array_start;
    params->virtual_array_length  = gfx_virtual_array_length;
    params->sync_group_hash       = vm_sync_u32_get_sync_group_hash();

    // override dimmer curve for the Pixie, since it already has curves built in
    if( pixel_u8_get_mode() == PIX_MODE_PIXIE ){

        params->dimmer_curve = 64;
    }
}

uint16_t gfx_u16_get_vm_frame_rate( void ){

    return gfx_frame_rate;
}

void gfx_v_set_pix_count( uint16_t setting ){

    pix_count = setting;
}

uint16_t gfx_u16_get_pix_count( void ){

    return pix_count;
}

void gfx_v_set_master_dimmer( uint16_t setting ){

    gfx_master_dimmer = setting;

    param_error_check();

    gfx_v_sync_params();
}

uint16_t gfx_u16_get_master_dimmer( void ){

    return gfx_master_dimmer;
}

void gfx_v_set_submaster_dimmer( uint16_t setting ){

    gfx_sub_dimmer = setting;

    param_error_check();

    gfx_v_sync_params();
}

uint16_t gfx_u16_get_submaster_dimmer( void ){

    return gfx_sub_dimmer;
}

// ISR(GFX_TIMER_CCA_vect){

// }

// ISR(GFX_TIMER_CCB_vect){

//     GFX_TIMER.CCB += vm_timer_rate;

//     run_flags |= FLAG_RUN_VM_LOOP;
// }

// ISR(GFX_TIMER_CCC_vect){

// }

void gfx_v_init( void ){

    if( pixel_u8_get_mode() == PIX_MODE_ANALOG ){

        // override size settings
        pix_count = 1;
        pix_size_x = 1;
        pix_size_y = 1;
    }

    param_error_check();

    // debug
    // PIXEL_EN_PORT.DIRSET = ( 1 << PIXEL_EN_PIN );
    // PIXEL_EN_PORT.OUTSET = ( 1 << PIXEL_EN_PIN );
    // PIX_CLK_PORT.DIRSET = ( 1 << PIX_CLK_PIN );
    // PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN );
    // PIX_DATA_PORT.DIRSET = ( 1 << PIX_DATA_PIN );
    // PIX_DATA_PORT.OUTCLR = ( 1 << PIX_DATA_PIN );

    // VM
    // update_vm_timer();
    // GFX_TIMER.CCB = vm_timer_rate;

    // GFX_TIMER.INTCTRLB = 0;
    // GFX_TIMER.INTCTRLB |= TC_CCAINTLVL_HI_gc;
    // GFX_TIMER.INTCTRLB |= TC_CCBINTLVL_HI_gc;
    // // GFX_TIMER.INTCTRLB |= TC_CCCINTLVL_HI_gc;

    pixel_v_init();

    thread_t_create( gfx_fader_thread,
                PSTR("gfx_fader"),
                0,
                0 );

    thread_t_create( gfx_vm_loop_thread,
                PSTR("gfx_vm_loop"),
                0,
                0 );
}

bool gfx_b_running( void ){

    return pixel_transfer_enable;
}

void gfx_v_set_sync0( uint16_t frame, uint32_t ts ){

    // vm0_sync_frame_number = frame;
    // vm0_sync_frame_ts = ts;

    // vm0_frame_number = frame;
    // vm0_frame_ts = ts;

/*

    // uint16_t rate = SYNC_RATE / gfx_frame_rate;
    // rate *= gfx_frame_rate;

    int16_t frame_offset = (int32_t)vm0_frame_number - (int32_t)frame;
    // last_vm0_frame_ts += ( frame_offset * gfx_frame_rate );

    // int32_t frame_offset = 0;
    int32_t time_offset = tmr_u32_elapsed_times( last_vm0_frame_ts, ts );

    // int32_t corrected_time_offset = time_offset + ( frame_offset * gfx_frame_rate );
    int32_t corrected_time_offset = time_offset;

    // we are ahead
    if( corrected_time_offset > 10 ){

        // slow down
        frame_rate_adjust = 4;
    }
    else if( corrected_time_offset > 2 ){

        // slow down
        frame_rate_adjust = 1;
    }
    // we are behind
    else if( corrected_time_offset < -10 ){

        // speed up
        frame_rate_adjust = -4;
    }
    else if( corrected_time_offset < -2 ){

        // speed up
        frame_rate_adjust = -1;
    }

    log_v_debug_P( PSTR("offset net: %ld frames: %d adj: %d"), time_offset, frame_offset, frame_rate_adjust );

    update_vm_timer();

    last_vm0_frame_ts = ts;
*/
}

void gfx_v_set_sync( uint16_t master_frame, uint32_t master_ts ){

    // int32_t frame_delta = (int32_t)master_frame - (int32_t)vm0_frame_number;
    // int32_t temp = frame_delta;

    // // master is ahead
    // if( frame_delta > 0 ){

    //     // rewind timestamp to match ours
    //     while( frame_delta > 0 ){

    //         frame_delta--;
    //         master_frame--;
    //         master_ts -= gfx_frame_rate;
    //     }
    // }
    // // we are ahead
    // else if( frame_delta < 0 ){

    //     // fastforwards timestamp to match ours
    //     while( frame_delta < 0 ){

    //         frame_delta++;
    //         master_frame++;
    //         master_ts += gfx_frame_rate;
    //     }
    // }

    // int32_t frame_offset = (int64_t)master_ts - (int64_t)vm0_frame_ts;

    // log_v_debug_P( PSTR("%ld %ld"), temp, frame_offset );

    // frame_rate_adjust = frame_offset;

    // vm0_sync_frame_number = master_frame;
    // vm0_sync_frame_ts = master_ts;


    // uint16_t master_frames_elapsed = (int32_t)master_frame - (int32_t)vm0_sync_frame_number;
    // uint32_t master_time_elapsed = tmr_u32_elapsed_times( vm0_sync_frame_ts, master_ts );
    // uint16_t master_rate = master_time_elapsed / master_frames_elapsed;

    // uint16_t our_frames_elapsed = (int32_t)vm0_frame_number - (int32_t)vm0_sync_frame_number;
    // uint32_t our_time_elapsed = tmr_u32_elapsed_times( vm0_sync_frame_ts, vm0_frame_ts );
    // uint16_t our_rate = our_time_elapsed / our_frames_elapsed;
    
    // // get rate delta
    // int16_t delta = master_rate - our_rate;

    // // log_v_debug_P( PSTR("master ts: %lu vm0: %lu elapsed: %lu"), master_ts, vm0_sync_frame_ts, master_time_elapsed );

    // log_v_debug_P( PSTR("master frames: %u elapsed: %lu rate: %u | our frames: %u elapsed: %lu rate %u | delta: %d"), 
    //     master_frames_elapsed, master_time_elapsed, master_rate, our_frames_elapsed, our_time_elapsed, our_rate, delta );

    // // update parameters for next sync
    // vm0_sync_frame_number = master_frame;
    // vm0_sync_frame_ts = master_ts;

    // // now figure out our offset
    // // we need to adjust our local frame counter to match the master's
    // // or rather, we adjust the master to match ours, using the master's timing data.

    // // get frame delta
    // int16_t frame_delta = vm0_frame_number - master_frame;
    
    // // delta is positive if we are ahead of master.  if so, master timestamp needs to increase
    // // by that many frames.
    // // the opposite will occur if we are behind.
    // uint32_t master_ts_adjusted = master_ts + ( (int32_t)frame_delta * master_rate );

    // int32_t true_offset = master_ts_adjusted - vm0_frame_ts;

    // // we are ahead
    // if( true_offset > 0 ){
        
    //     if( true_offset > 50 ){

    //         // slow down
    //         frame_rate_adjust = 20;
    //     }
    //     else if( true_offset > 10 ){

    //         // slow down
    //         frame_rate_adjust = 4;
    //     }
    //     else if( true_offset > 2 ){

    //         // slow down
    //         frame_rate_adjust = 1;
    //     }
    // }
    // // we are behind
    // else if( true_offset < 0 ){
        
    //     if( true_offset < -15 ){

    //         // speed up
    //         frame_rate_adjust = -20;
    //     }
    //     else if( true_offset < -10 ){

    //         // speed up
    //         frame_rate_adjust = -4;
    //     }
    //     else if( true_offset < -2 ){

    //         // speed up
    //         frame_rate_adjust = -1;
    //     }
    // }

    // frame_rate_adjust = 0;

    // log_v_debug_P( PSTR("frame delta: %d master adjusted: %lu true offset: %ld adj: %d"), frame_delta, master_ts_adjusted, true_offset, frame_rate_adjust );

    // update_vm_timer();
}


void gfx_v_pixel_bridge_enable( void ){

    pixel_transfer_enable = TRUE;
}

void gfx_v_pixel_bridge_disable( void ){

    pixel_transfer_enable = FALSE;
}

void gfx_v_sync_params( void ){

    gfx_params_t params;

    gfx_v_get_params( &params );

    wifi_i8_send_msg( WIFI_DATA_ID_GFX_PARAMS, (uint8_t *)&params, sizeof(params) );
}

// int8_t wifi_i8_msg_handler( uint8_t data_id, uint8_t *data, uint16_t len ){
    
//     if( data_id == WIFI_DATA_ID_VM_INFO ){

//         if( len != sizeof(wifi_msg_vm_info_t) ){

//             return -1;
//         }

//         wifi_msg_vm_info_t *msg = (wifi_msg_vm_info_t *)data;

//         vm_v_received_info( msg );
//     }
//     else if( ( data_id == WIFI_DATA_ID_VM_FRAME_SYNC ) ||
//              ( data_id == WIFI_DATA_ID_VM_SYNC_DATA ) ||
//              ( data_id == WIFI_DATA_ID_VM_SYNC_DONE ) ){

//         vm_sync_v_process_msg( data_id, data, len );
//     }

//     return 0;    
// }

void gfx_v_subscribe_key( catbus_hash_t32 hash, uint8_t tag ){

    int8_t empty = -1;
    for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

        if( subscribed_keys[i].hash == hash ){

            subscribed_keys[i].tag |= tag;
            return;
        }
        else if( ( subscribed_keys[i].hash == 0 ) && ( empty < 0 ) ){

            empty = i;
        }
    }

    if( empty < 0 ){

        log_v_debug_P( PSTR("subscribed keys full") );

        return;
    }

    subscribed_keys[empty].hash     = hash;
    subscribed_keys[empty].tag      = tag;
    subscribed_keys[empty].flags    = KEY_FLAG_UPDATED;
}


void gfx_v_reset_subscribed( uint8_t tag ){

    for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

        if( subscribed_keys[i].tag & tag ){

            subscribed_keys[i].tag &= ~tag;

            if( subscribed_keys[i].tag == 0 ){

                subscribed_keys[i].hash = 0;
            } 
        }
    }  
}

void kv_v_notify_hash_set( catbus_hash_t32 hash ){

    for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

        if( subscribed_keys[i].hash == hash ){

            subscribed_keys[i].flags |= KEY_FLAG_UPDATED;

            break;
        }
    }   
}


void gfx_v_sync_db( bool all ){

    for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

        if( subscribed_keys[i].hash == 0 ){

            continue;
        }

        if( ( subscribed_keys[i].flags == 0 ) && !all ){

            continue;
        }

        subscribed_keys[i].flags = 0;

        kv_meta_t meta;
        if( kv_i8_lookup_hash( subscribed_keys[i].hash, &meta ) < 0 ){

            continue;
        }

        // uint8_t buf[CATBUS_MAX_DATA + sizeof(wifi_msg_kv_data_t)];
        uint8_t buf[128 + sizeof(wifi_msg_kv_data_t)];
        wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)buf;
        uint8_t *data = (uint8_t *)( msg + 1 );
    
        if( kv_i8_internal_get( &meta, meta.hash, 0, 0, data, CATBUS_MAX_DATA ) < 0 ){

            continue;
        }  

        uint16_t data_len = type_u16_size( meta.type ) * ( (uint16_t)meta.array_len + 1 );

        msg->meta.hash        = meta.hash;
        msg->meta.type        = meta.type;
        msg->meta.count       = meta.array_len;
        msg->meta.flags       = meta.flags;
        msg->meta.reserved    = 0;
        msg->tag              = subscribed_keys[i].tag;

        if( wifi_i8_send_msg( WIFI_DATA_ID_KV_DATA, buf, data_len + sizeof(wifi_msg_kv_data_t) ) < 0 ){

            continue;
        }   
    }
}

void gfx_v_read_db_key( uint32_t hash ){

    if( wifi_i8_send_msg( WIFI_DATA_ID_GET_KV_DATA, (uint8_t *)&hash, sizeof(hash) ) < 0 ){

        return;
    }

    uint8_t buf[GFX_MAX_DB_LEN + sizeof(wifi_msg_kv_data_t)];
    wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)buf;   

    uint16_t bytes_read = 0;

    if( wifi_i8_receive_msg( WIFI_DATA_ID_GET_KV_DATA, (uint8_t *)buf, sizeof(buf), &bytes_read ) < 0 ){

        return;
    }

    if( bytes_read < sizeof(wifi_msg_kv_data_t) ){

        return;
    }

    uint8_t *kv_data = (uint8_t *)( msg + 1 );

    catbus_i8_array_set( msg->meta.hash, msg->meta.type, 0, msg->meta.count + 1, kv_data, 0 );
}   

void gfx_v_read_db( void ){

    for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

        if( subscribed_keys[i].hash == 0 ){

            continue;
        }

        gfx_v_read_db_key( subscribed_keys[i].hash );
    }
}

static bool should_halt_fader( void ){

    if( !wifi_b_attached() ){
        return TRUE;
    }

    return 
        ( pixel_u8_get_mode() == PIX_MODE_OFF ) ||
        ( gfx_u16_get_pix_count() == 0 ) ||
        ( !vm_b_running() ) ||
        ( !gfx_b_running() );
}

PT_THREAD( gfx_fader_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    THREAD_WAIT_WHILE( pt, should_halt_fader() );

    // init alarm
    thread_v_set_alarm( tmr_u32_get_system_time_ms() );
    
    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + FADER_RATE );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        // check if shutting down
        if( wifi_b_shutdown() ){
            
            pixel_v_clear();

            uint16_t h = 0;
            uint16_t s = 0;
            uint16_t v = 4096;

            pixel_v_load_hsv( 0, 1, &h, &s, &v );

            THREAD_EXIT( pt );
        }

        if( should_halt_fader() ){

            THREAD_RESTART( pt );            
        }

        if( wifi_i8_send_msg( WIFI_DATA_ID_RUN_FADER, 0, 0 ) < 0 ){

            THREAD_RESTART( pt );
        }

        wifi_msg_fader_info_t fader_info;

        if( wifi_i8_receive_msg( WIFI_DATA_ID_RUN_FADER, (uint8_t *)&fader_info, sizeof(fader_info), 0 ) < 0 ){

            THREAD_RESTART( pt );
        }

        vm_fader_time = fader_info.fader_time;

        // get pixel data
        #ifdef USE_HSV_BRIDGE

        uint8_t pages = ( ( gfx_u16_get_pix_count() - 1 ) / WIFI_HSV_DATA_N_PIXELS ) + 1;

        for( uint8_t page = 0; page < pages; page++ ){

            if( wifi_i8_send_msg( WIFI_DATA_ID_HSV_ARRAY, &page, sizeof(page) ) < 0 ){

                THREAD_RESTART( pt );
            }

            wifi_msg_hsv_array_t msg;   
            uint16_t bytes_read;

            if( wifi_i8_receive_msg( WIFI_DATA_ID_HSV_ARRAY, (uint8_t *)&msg, sizeof(msg), &bytes_read ) < 0 ){

                THREAD_RESTART( pt );
            }

            // unpack HSV pointers
            uint16_t *h = (uint16_t *)msg.hsv_array;
            uint16_t *s = h + msg.count;
            uint16_t *v = s + msg.count;

            pixel_v_load_hsv( msg.index, msg.count, h, s, v );
        }

        #else

        if( pixel_u8_get_mode() == PIX_MODE_ANALOG ){

            if( wifi_i8_send_msg( WIFI_DATA_ID_RGB_PIX0, 0, 0 ) < 0 ){

                THREAD_RESTART( pt );
            }

            wifi_msg_rgb_pix0_t msg_pix0;
            uint16_t bytes_read;

            if( wifi_i8_receive_msg( WIFI_DATA_ID_RGB_PIX0, (uint8_t *)&msg_pix0, sizeof(msg_pix0), &bytes_read ) < 0 ){

                THREAD_RESTART( pt );
            }

            pixel_v_set_analog_rgb( msg_pix0.r, msg_pix0.g, msg_pix0.b );
        }
        else{
        
            uint8_t pages = ( ( gfx_u16_get_pix_count() - 1 ) / WIFI_RGB_DATA_N_PIXELS ) + 1;

            for( uint8_t page = 0; page < pages; page++ ){

                if( wifi_i8_send_msg( WIFI_DATA_ID_RGB_ARRAY, &page, sizeof(page) ) < 0 ){

                    THREAD_RESTART( pt );
                }

                wifi_msg_rgb_array_t msg;   
                uint16_t bytes_read;

                if( wifi_i8_receive_msg( WIFI_DATA_ID_RGB_ARRAY, (uint8_t *)&msg, sizeof(msg), &bytes_read ) < 0 ){

                    THREAD_RESTART( pt );
                }

                // unpack RGBD pointers
                uint8_t *r = msg.rgbd_array;
                uint8_t *g = r + msg.count;
                uint8_t *b = g + msg.count;
                uint8_t *d = b + msg.count;

                pixel_v_load_rgb( msg.index, msg.count, r, g, b, d );   
            }
        }

        #endif
    }
            
PT_END( pt );
}


PT_THREAD( gfx_vm_loop_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    static uint32_t net_time;

    // init alarm
    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while(1){

        THREAD_WAIT_WHILE( pt, !vm_b_running() );

        if( vm_sync_b_is_synced() ){

            net_time = time_u32_get_network_aligned( gfx_frame_rate );
            THREAD_WAIT_WHILE( pt, !update_frame_rate && ( time_i8_compare_network_time( net_time ) > 0 )  );
        }
        else{

            thread_v_set_alarm( thread_u32_get_alarm() + gfx_frame_rate );
            THREAD_WAIT_WHILE( pt, !update_frame_rate && thread_b_alarm_set() );
        }

        // check if shutting down
        if( wifi_b_shutdown() ){

            THREAD_EXIT( pt );
        }

        if( update_frame_rate ){

            update_frame_rate = FALSE;
            THREAD_RESTART( pt );            
        }

        if( vm_i8_run_loops() < 0 ){

            // comm fail

            // let's delay
            TMR_WAIT( pt, 100 );

            THREAD_RESTART( pt );            
        }

        vm0_frame_ts = time_u32_get_network_time();
        vm0_frame_number++;

        vm_sync_v_frame_trigger();

        uint16_t rate = SYNC_RATE / gfx_frame_rate;

        if( ( vm0_frame_number % rate ) == 0 ){

            vm_sync_v_trigger();
        }
    }

PT_END( pt );
}


// PT_THREAD( gfx_vm_loop_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
    
//     while(1){

//         THREAD_WAIT_WHILE( pt, ( run_flags == 0 ) || !vm_b_running() );

//         // check if shutting down
//         if( wifi_b_shutdown() ){

//             THREAD_EXIT( pt );
//         }

//         ATOMIC;
//         uint8_t flags = run_flags;
//         run_flags = 0;
//         END_ATOMIC;

//         if( flags & FLAG_RUN_VM_LOOP ){

//             if( vm_i8_run_loops() < 0 ){

//                 // comm fail

//                 // let's delay
//                 TMR_WAIT( pt, 100 );
//             }

//             vm0_frame_ts = time_u32_get_network_time();
//             vm0_frame_number++;
//             // last_vm0_frame_ts += gfx_frame_rate;

//             // if( vm_sync_b_is_slave() ){

//             //     uint32_t net_time = time_u32_get_network_time();
//             // }

//             vm_sync_v_frame_trigger();

//             uint16_t rate = SYNC_RATE / gfx_frame_rate;

//             if( ( vm0_frame_number % rate ) == 0 ){

//                 vm_sync_v_trigger();
//             }
//         }
//     }
            
// PT_END( pt );
// }
