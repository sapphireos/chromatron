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

#include "system.h"
#include "logging.h"
#include "keyvalue.h"
#include "threading.h"
#include "timers.h"
#include "fs.h"

#include "pixel.h"
#include "graphics.h"
#include "battery.h"
#include "vm.h"
#include "vm_sync.h"
#include "superconductor.h"


// pixel calibrations for a single pixel at full power
#define MICROAMPS_RED_PIX       10000
#define MICROAMPS_GREEN_PIX     10000
#define MICROAMPS_BLUE_PIX      10000
#define MICROAMPS_WHITE_PIX     20000
#define MICROAMPS_IDLE_PIX       1000 // idle power for an unlit pixel

#define PIXEL_MILLIVOLTS        5000

#define PIXEL_HEADROOM_WARNING  500 // warn if within this milliwatts of max power


static uint16_t vm_fader_time;
static uint32_t pixel_power;
static uint16_t pix_max_power; // in milliwatts

static uint32_t worst_timing_lag;
static uint32_t timing_resyncs;

KV_SECTION_META kv_meta_t gfx_info_kv[] = {
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_fader_time,        0,                  "vm_fade_time" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST,    &pix_max_power,        0,                  "pix_max_power" },
    { CATBUS_TYPE_UINT32,   0, 0,                   &worst_timing_lag,     0,                  "gfx_timing_lag" },
    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY,  &timing_resyncs,       0,                  "gfx_timing_resyncs" },
};

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );


static uint8_t fx_rainbow[] __attribute__((aligned(4))) = {
    #include "rainbow.fx.carray"
};


static uint32_t fx_rainbow_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    uint32_t ret_val = len;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){
        case FS_VFILE_OP_READ:
            memcpy( ptr, &fx_rainbow[pos], len );
            break;

        case FS_VFILE_OP_SIZE:
            ret_val = sizeof(fx_rainbow);
            break;

        default:
            ret_val = 0;
            break;
    }

    return ret_val;
}


void gfx_v_init( void ){

    gfxlib_v_init();

    pixel_v_init();

    #ifdef ENABLE_TIME_SYNC
    vm_sync_v_init();
    #endif

    sc_v_init();

    fs_f_create_virtual( PSTR("_rainbow.fxb"), fx_rainbow_vfile_handler );

    thread_t_create( gfx_control_thread,
                PSTR("gfx_control"),
                0,
                0 );
}

uint32_t gfx_u32_get_pixel_power( void ){

    return pixel_power;
}

static void calc_pixel_power( void ){

    uint64_t power_temp;

    // update pixel power
    if( batt_b_pixels_enabled() ){

        power_temp = gfx_u16_get_pix_count() * MICROAMPS_IDLE_PIX;
        power_temp += ( gfx_u32_get_pixel_r() * MICROAMPS_RED_PIX ) / 256;
        power_temp += ( gfx_u32_get_pixel_g() * MICROAMPS_GREEN_PIX ) / 256;
        power_temp += ( gfx_u32_get_pixel_b() * MICROAMPS_BLUE_PIX ) / 256;
        power_temp += ( gfx_u32_get_pixel_w() * MICROAMPS_WHITE_PIX ) / 256;

        // multiply by voltage to get power in microwatts
        power_temp *= PIXEL_MILLIVOLTS;
        power_temp /= 1000;
    }
    else{

        power_temp = 0;
    }

    pixel_power = power_temp;
}

static void apply_power_limit( void ){

    // if max power is not configured, do nothing:
    if( pix_max_power == 0 ){

        return;
    }

    // convert pixel power from microwatts to milliwatts
    uint32_t pixel_power_mw = pixel_power / 1000;

    int32_t power_delta = (int32_t)pixel_power_mw - (int32_t)pix_max_power;

    // if power_delta is positive, we have exceeded the power limit by the amount
    // in power_delta

    // if power_delta is negative, power_delta indicates how much headroom 
    // is available

    if( power_delta > 0 ){

        log_v_error_P( PSTR("Pixel power limit exceeded by %d mW"), power_delta );

        gfx_v_power_limiter_graphic();
    }
    else if( power_delta > -PIXEL_HEADROOM_WARNING ){

        log_v_warn_P( PSTR("Pixel power headroom remaining: %d mW"), -1 * power_delta );
    }
}

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // init alarm
    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    // gfx_v_log_value_curve();
    // THREAD_EXIT( pt );

    // init pixel arrays
    gfx_v_process_faders();
    gfx_v_sync_array();
    calc_pixel_power();
    pixel_v_signal();

    thread_v_create_timed_signal( GFX_SIGNAL_0, 20 );

    static uint32_t start;
    start = tmr_u32_get_system_time_us();

    while(1){        

        // thread_v_set_alarm( thread_u32_get_alarm() + FADER_RATE );
        // THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );
        THREAD_WAIT_SIGNAL( pt, GFX_SIGNAL_0 );

        // int32_t lag = (int64_t)tmr_u32_get_system_time_ms() - (int64_t)thread_u32_get_alarm();
        // uint32_t lag = tmr_u32_elapsed_time_ms( thread_u32_get_alarm( );
        uint32_t lag = tmr_u32_elapsed_time_us( start );

        if( lag > worst_timing_lag ){

            worst_timing_lag = lag;
        }


        start = tmr_u32_get_system_time_us();

        // if( lag > 5 ){

        //     timing_resyncs++;

        //     // log_v_debug_P( PSTR("Graphics timing lag: %u"), lag );

        //     // reset alarm setting to re-align timing
        //     thread_v_set_alarm( tmr_u32_get_system_time_ms() );
        // }



            io_v_set_mode( IO_PIN_18_MOSI, IO_MODE_OUTPUT );
            
            io_v_digital_write( IO_PIN_18_MOSI, TRUE );
            _delay_us( 1 );
            io_v_digital_write( IO_PIN_18_MOSI, FALSE );


        // if( sys_b_is_shutting_down() ){

        //     gfx_v_shutdown_graphic();
        //     calc_pixel_power();
        //     apply_power_limit();
            
        //     pixel_v_signal();        

        //     THREAD_EXIT( pt );
        // }

        // if( vm_b_running() ){

        //     gfx_v_process_faders();
        //     calc_pixel_power();
        //     apply_power_limit();

        //     gfx_v_sync_array();

        //     pixel_v_signal();
        // }

        uint32_t elapsed = tmr_u32_elapsed_time_us( start );

        vm_fader_time = elapsed;
    }

PT_END( pt );
}
