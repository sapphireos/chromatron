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
#include "catbus_common.h"
#include "catbus.h"
#include "catbus_packer.h"

#include "trig.h"
#include "pixel.h"
#include "graphics.h"
#include "esp8266.h"
#include "vm.h"
#include "timesync.h"
#include "kvdb.h"
#include "hash.h"
#include "vm_wifi_cmd.h"




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

static mem_handle_t subscribed_keys_h = -1;

static volatile uint8_t run_flags;
#define FLAG_RUN_PARAMS         0x01
#define FLAG_RUN_VM_LOOP        0x02
#define FLAG_RUN_FADER          0x04

static uint16_t vm_timer_rate; 


static uint16_t calc_vm_timer( uint32_t ms ){

    return ( ms * 31250 ) / 1000;
}

static void update_vm_timer( void ){

    uint16_t new_timer = calc_vm_timer( gfx_frame_rate );

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

static uint32_t get_sync_group_hash( void ){

    char sync_group[32];
    if( kv_i8_get( __KV__gfx_sync_group, sync_group, sizeof(sync_group) ) < 0 ){

        return 0;
    }

    return hash_u32_string( sync_group );    
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
    { SAPPHIRE_TYPE_STRING32,   0, KV_FLAGS_PERSIST, 0,                            gfx_i8_kv_handler,   "gfx_sync_group" },
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
    params->pix_mode            = pixel_u8_get_mode();

    params->virtual_array_start   = gfx_virtual_array_start;
    params->virtual_array_length  = gfx_virtual_array_length;
    params->sync_group_hash       = get_sync_group_hash();

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


PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );
PT_THREAD( gfx_db_xfer_thread( pt_t *pt, void *state ) );

#define FADER_TIMER_RATE 625 // 20 ms

#define PARAMS_TIMER_RATE 1000 // 1000 ms

#define STROBE_CLK PIX_CLK_PORT.OUTSET = ( 1 << PIX_CLK_PIN ); \
                   _delay_us(10); \
                   PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN )

#define STROBE_DATA PIX_DATA_PORT.OUTSET = ( 1 << PIX_DATA_PIN ); \
                    _delay_us(10); \
                    PIX_DATA_PORT.OUTCLR = ( 1 << PIX_DATA_PIN )


ISR(GFX_TIMER_CCA_vect){

    GFX_TIMER.CCA += FADER_TIMER_RATE;

    run_flags |= FLAG_RUN_FADER;

    // STROBE_CLK;
}

ISR(GFX_TIMER_CCB_vect){

    GFX_TIMER.CCB += vm_timer_rate;

    run_flags |= FLAG_RUN_VM_LOOP;

    // STROBE_DATA;
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

int8_t gfx_i8_send_keys( catbus_hash_t32 *hash, uint8_t count ){

    uint8_t buf[WIFI_MAX_DATA_LEN];

    uint8_t buf_ptr = 0;

    while( count > 0 ){

        catbus_pack_ctx_t ctx;
        if( catbus_i8_init_pack_ctx( *hash, &ctx ) < 0 ){

            count--;
            hash++;
            continue;
        }

        int16_t packed = -1;

        do{
            packed = catbus_i16_pack( &ctx, &buf[buf_ptr], sizeof(buf) - buf_ptr );
    
            if( packed < 0 ){

                wifi_i8_send_msg_blocking( WIFI_DATA_ID_KV_DATA, buf, buf_ptr );  

                buf_ptr = 0;
            }
            else{

                buf_ptr += packed;
            }

        } while( !catbus_b_pack_complete( &ctx ) );

        if( catbus_b_pack_complete( &ctx ) ){

            count--;
            hash++;
        }

        // check if buffer full
        if( ( buf_ptr >= sizeof(buf) ) || ( count == 0 ) || ( packed < 0 ) ){

            wifi_i8_send_msg_blocking( WIFI_DATA_ID_KV_DATA, buf, buf_ptr );  
            // log_v_debug_P( PSTR("send len: %d"), buf_ptr );

            buf_ptr = 0;
        }        
    }

    return 0;
}

static int8_t send_read_keys( void ){

    if( subscribed_keys_h < 0 ){

        return 0;
    }

    uint32_t read_keys_count = mem2_u16_get_size( subscribed_keys_h ) / sizeof(uint32_t);
    uint32_t *read_hash = mem2_vp_get_ptr_fast( subscribed_keys_h );

    return gfx_i8_send_keys( read_hash, read_keys_count );
}

static int8_t send_run_fader_cmd( void ){

    return wifi_i8_send_msg( WIFI_DATA_ID_RUN_FADER, 0, 0 );   
}


int8_t wifi_i8_msg_handler( uint8_t data_id, uint8_t *data, uint8_t len ){
    
    
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

        int16_t status = 0;

        while( status >= 0 ){

            data += status;
            len -= status;
            status = catbus_i16_unpack( data, len );       

            // log_v_debug_P( PSTR("KV sts: %d len %d"), 
                // status, len );      
        }        
    }
    else if( data_id == WIFI_DATA_ID_DEBUG_PRINT ){

        log_v_debug_P( PSTR("ESP: %s"), data );
    }

    return 0;    
}



void gfx_v_sync_params( void ){

    send_params( TRUE );    
}

void gfx_v_set_subscribed_keys( mem_handle_t h ){

    subscribed_keys_h = h;
}

void gfx_v_reset_subscribed( void ){

    if( subscribed_keys_h > 0 ){

        mem2_v_free( subscribed_keys_h );
    }

    subscribed_keys_h = -1;
}


PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    

    // test_packer();
    
    static uint32_t last_param_sync;
    static uint8_t flags;

    // wait until wifi is attached before starting up pixel driver
    THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

    param_error_check();  



    // kvdb_i8_add( __KV__test_array, CATBUS_TYPE_UINT16, 32, 0, 0 );
    // kvdb_v_set_name_P( PSTR("test_array") );
    
    // kvdb_i8_add( __KV__test_data,  CATBUS_TYPE_UINT32, 1, 0, 0, "test_data" );
    // kvdb_i8_add( __KV__test_meow,  CATBUS_TYPE_UINT32, 16, 0, 0, "test_meow" );

    // catbus_hash_t32 read_keys[] = {
    //     __KV__test_array,
    //     __KV__test_data,
    //     __KV__test_meow,
    // };
    
    // subscribed_keys_h = mem2_h_alloc( sizeof(read_keys) );
    // memcpy( mem2_vp_get_ptr_fast(subscribed_keys_h), read_keys, sizeof(read_keys) );

    // send_read_keys();


    while(1){

        THREAD_WAIT_WHILE( pt, ( run_flags == 0 ) && 
                               ( tmr_u32_elapsed_time_ms( last_param_sync ) < PARAMS_TIMER_RATE ) );

        ATOMIC;
        flags = run_flags;
        run_flags = 0;
        END_ATOMIC;

        // log_v_debug_P( PSTR("%x"), flags );

        if( flags & FLAG_RUN_VM_LOOP ){

            // check if vm is running
            if( !vm_b_running() ){

                goto end;
            }

            
            THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
            send_read_keys();

            THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );
            send_run_vm_cmd();
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

            last_param_sync = tmr_u32_get_system_time_ms();
        }

        THREAD_YIELD( pt ); 
    }

PT_END( pt );
}



void kv_v_notify_hash_set( uint32_t hash ){

    
}


PT_THREAD( gfx_db_xfer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // static catbus_pack_ctx_t ctx;
    // static uint16_t kv_index;
    
    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_comm_ready() );



        THREAD_YIELD( pt );
    }
        
PT_END( pt );
}





