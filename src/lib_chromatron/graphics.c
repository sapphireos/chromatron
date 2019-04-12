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
static uint8_t gfx_dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;
static uint8_t gfx_sat_curve = GFX_SAT_CURVE_DEFAULT;

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

#define PARAMS_TIMER_RATE 100 // 100 ms (system ms timer)

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );
PT_THREAD( gfx_db_xfer_thread( pt_t *pt, void *state ) );

typedef struct{
    catbus_hash_t32 hash;
    uint8_t tag;
    uint8_t flags;
} subscribed_key_t;
#define KEY_FLAG_UPDATED        0x01

static bool run_xfer;
static subscribed_key_t subscribed_keys[32];


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
        GFX_TIMER.CCB = GFX_TIMER.CNT + vm_timer_rate;

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
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &gfx_sat_curve,               gfx_i8_kv_handler,   "gfx_sat_curve" },
    
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_virtual_array_start,     gfx_i8_kv_handler,   "gfx_varray_start" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_virtual_array_length,    gfx_i8_kv_handler,   "gfx_varray_length" },
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

    ATOMIC;
    run_flags |= FLAG_RUN_PARAMS;
    END_ATOMIC;
}

uint16_t gfx_u16_get_master_dimmer( void ){

    return gfx_master_dimmer;
}

void gfx_v_set_submaster_dimmer( uint16_t setting ){

    gfx_sub_dimmer = setting;

    param_error_check();

    ATOMIC;
    run_flags |= FLAG_RUN_PARAMS;
    END_ATOMIC;
}

uint16_t gfx_u16_get_submaster_dimmer( void ){

    return gfx_sub_dimmer;
}

ISR(GFX_TIMER_CCA_vect){

    GFX_TIMER.CCA += FADER_TIMER_RATE;

    run_flags |= FLAG_RUN_FADER;
}

ISR(GFX_TIMER_CCB_vect){

    GFX_TIMER.CCB += vm_timer_rate;

    run_flags |= FLAG_RUN_VM_LOOP;
}

ISR(GFX_TIMER_CCC_vect){

}

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

    // fader
    GFX_TIMER.CCA = FADER_TIMER_RATE;

    // VM
    update_vm_timer();
    GFX_TIMER.CCB = vm_timer_rate;

    GFX_TIMER.INTCTRLB = 0;
    GFX_TIMER.INTCTRLB |= TC_CCAINTLVL_HI_gc;
    GFX_TIMER.INTCTRLB |= TC_CCBINTLVL_HI_gc;
    // GFX_TIMER.INTCTRLB |= TC_CCCINTLVL_HI_gc;

    GFX_TIMER.CTRLA = TC_CLKSEL_DIV1024_gc;
    GFX_TIMER.CTRLB = 0;

    pixel_v_init();


    thread_t_create( gfx_control_thread,
                PSTR("gfx_control"),
                0,
                0 );

    thread_t_create( gfx_db_xfer_thread,
                PSTR("gfx_db_xfer"),
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

void gfx_v_pixel_bridge_enable( void ){

    pixel_transfer_enable = TRUE;
}

void gfx_v_pixel_bridge_disable( void ){

    pixel_transfer_enable = FALSE;
}

static int8_t send_params( bool blocking ){

    gfx_params_t params;

    gfx_v_get_params( &params );

    if( blocking ){

        return wifi_i8_send_msg_blocking( WIFI_DATA_ID_GFX_PARAMS, (uint8_t *)&params, sizeof(params) );
    }
    else{
     
        return wifi_i8_send_msg( WIFI_DATA_ID_GFX_PARAMS, (uint8_t *)&params, sizeof(params) );
    }
}

static int8_t send_run_vm_cmd( void ){

    return wifi_i8_send_msg( WIFI_DATA_ID_RUN_VM, 0, 0 );   
}

static int8_t send_run_fader_cmd( void ){

    return wifi_i8_send_msg( WIFI_DATA_ID_RUN_FADER, 0, 0 );   
}


int8_t wifi_i8_msg_handler( uint8_t data_id, uint8_t *data, uint16_t len ){
    
    
    #ifdef USE_HSV_BRIDGE
    if( data_id == WIFI_DATA_ID_HSV_ARRAY ){

        if( pixel_transfer_enable ){

            wifi_msg_hsv_array_t *msg = (wifi_msg_hsv_array_t *)data;

            // // unpack HSV pointers
            uint16_t *h = (uint16_t *)msg->hsv_array;
            uint16_t *s = h + msg->count;
            uint16_t *v = s + msg->count;

            pixel_v_load_hsv( msg->index, msg->count, h, s, v );   
        }
    }
    #else
    if( data_id == WIFI_DATA_ID_RGB_PIX0 ){

        if( pixel_transfer_enable ){
            
            if( len != sizeof(wifi_msg_rgb_pix0_t) ){

                return -1;
            }

            wifi_msg_rgb_pix0_t *msg = (wifi_msg_rgb_pix0_t *)data;

            pixel_v_set_analog_rgb( msg->r, msg->g, msg->b );
        }
    }  
    else if( data_id == WIFI_DATA_ID_RGB_ARRAY ){

        if( pixel_transfer_enable ){

            wifi_msg_rgb_array_t *msg = (wifi_msg_rgb_array_t *)data;

            // unpack RGBD pointers
            uint8_t *r = msg->rgbd_array;
            uint8_t *g = r + msg->count;
            uint8_t *b = g + msg->count;
            uint8_t *d = b + msg->count;

            pixel_v_load_rgb( msg->index, msg->count, r, g, b, d );   
        }
    }
    #endif
    else if( data_id == WIFI_DATA_ID_VM_INFO ){

        if( len != sizeof(wifi_msg_vm_info_t) ){

            return -1;
        }

        wifi_msg_vm_info_t *msg = (wifi_msg_vm_info_t *)data;

        vm_v_received_info( msg );
    }
    else if( data_id == WIFI_DATA_ID_KV_DATA ){

        wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)data;
        uint8_t *kv_data = (uint8_t *)( msg + 1 );

        catbus_i8_array_set( msg->meta.hash, msg->meta.type, 0, msg->meta.count + 1, kv_data, 0 );
    }
    else if( ( data_id == WIFI_DATA_ID_VM_FRAME_SYNC ) ||
             ( data_id == WIFI_DATA_ID_VM_SYNC_DATA ) ||
             ( data_id == WIFI_DATA_ID_VM_SYNC_DONE ) ){

        vm_sync_v_process_msg( data_id, data, len );
    }

    return 0;    
}

void gfx_v_sync_params( void ){
    
    send_params( TRUE );    
}

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    static uint32_t last_param_sync;
    static uint8_t flags;

    // wait until wifi is attached before starting up pixel driver
    THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

    param_error_check();  


    while(1){

        THREAD_WAIT_WHILE( pt, ( run_flags == 0 ) && 
                               ( tmr_u32_elapsed_time_ms( last_param_sync ) < PARAMS_TIMER_RATE ) );

        ATOMIC;
        flags = run_flags;
        run_flags = 0;
        END_ATOMIC;

        if( flags & FLAG_RUN_VM_LOOP ){

            // check if vm is running
            if( !vm_b_running() ){

                goto end;
            }

            THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
            send_run_vm_cmd();

            if( vm_b_is_vm_running( 0 ) ){
                
                vm0_frame_number++;
                last_vm0_frame_ts = time_u32_get_network_time();
            }
        }

end:
        if( flags & FLAG_RUN_FADER ){   

            if( pixel_u8_get_mode() != PIX_MODE_OFF ){
                
                THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
                send_run_fader_cmd();
            }
        }

        if( ( flags & FLAG_RUN_PARAMS ) ||
            ( tmr_u32_elapsed_time_ms( last_param_sync ) >= PARAMS_TIMER_RATE ) ){

            THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
            send_params( FALSE );

            // run DB transfer
            run_xfer = TRUE;
            for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

                if( subscribed_keys[i].hash == 0 ){

                   continue;
                }
                    
                subscribed_keys[i].flags |= KEY_FLAG_UPDATED;
            }   

            last_param_sync = tmr_u32_get_system_time_ms();
        }

        THREAD_YIELD( pt ); 
    }

PT_END( pt );
}
    

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

    run_xfer = TRUE;
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

void gfx_v_init_vm( uint8_t vm_id ){

    run_xfer = TRUE;

    init_vm |= ( 1 << vm_id );

    if( vm_id == 0 ){

        vm0_frame_number = 0;
    }
}


void kv_v_notify_hash_set( catbus_hash_t32 hash ){

    for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

        if( subscribed_keys[i].hash == hash ){

            subscribed_keys[i].flags |= KEY_FLAG_UPDATED;

            run_xfer = TRUE;

            break;
        }
    }   
}


PT_THREAD( gfx_db_xfer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint8_t index;


    THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

    while(1){

        THREAD_WAIT_WHILE( pt, !run_xfer );

        while( index < cnt_of_array(subscribed_keys) ){

            if( subscribed_keys[index].hash == 0 ){

                goto end;
            }

            if( subscribed_keys[index].flags == 0 ){

                goto end;
            }

            THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );

            subscribed_keys[index].flags = 0;

            kv_meta_t meta;
            if( kv_i8_lookup_hash( subscribed_keys[index].hash, &meta, 0 ) < 0 ){

                goto end;
            }

            // uint8_t buf[CATBUS_MAX_DATA + sizeof(wifi_msg_kv_data_t)];
            uint8_t buf[128 + sizeof(wifi_msg_kv_data_t)];
            wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)buf;
            uint8_t *data = (uint8_t *)( msg + 1 );
        
            if( kv_i8_internal_get( &meta, meta.hash, 0, 0, data, CATBUS_MAX_DATA ) < 0 ){

                goto end;
            }  

            uint16_t data_len = type_u16_size( meta.type ) * ( (uint16_t)meta.array_len + 1 );

            msg->meta.hash        = meta.hash;
            msg->meta.type        = meta.type;
            msg->meta.count       = meta.array_len;
            msg->meta.flags       = meta.flags;
            msg->meta.reserved    = 0;
            msg->tag              = subscribed_keys[index].tag;

            wifi_i8_send_msg( WIFI_DATA_ID_KV_DATA, buf, data_len + sizeof(wifi_msg_kv_data_t) );


end:
            index++;
        }

        index = 0;
        run_xfer = FALSE;

        if( init_vm != 0 ){

            THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );

            uint32_t vm_id = init_vm;

            wifi_i8_send_msg( WIFI_DATA_ID_INIT_VM, (uint8_t *)&vm_id, sizeof(vm_id) );
            init_vm = 0;
        }

        // check if any flags are set
        for( uint8_t i = 0; i < cnt_of_array(subscribed_keys); i++ ){

            if( subscribed_keys[i].hash == 0 ){

                continue;
            }

            if( subscribed_keys[i].flags & KEY_FLAG_UPDATED ){

                run_xfer = TRUE;
                break;
            }
        }

    }
        
PT_END( pt );
}

