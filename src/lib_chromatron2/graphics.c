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
#include "hal_pixel.h"
#include "graphics.h"
#include "vm.h"
#include "timesync.h"
#include "kvdb.h"
#include "hash.h"


static uint16_t vm0_frame_number;
static uint32_t last_vm0_frame_ts;
static int16_t frame_rate_adjust;
static uint16_t vm_fader_time;

KV_SECTION_META kv_meta_t gfx_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_fader_time,        0,                  "vm_fade_time" },
};

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );


void gfx_v_init( void ){

    if( pixel_u8_get_mode() == PIX_MODE_ANALOG ){

        log_v_warn_P( PSTR("Analog mode not supported on this hardware") );
    }

    gfxlib_v_init();

    pixel_v_init();

    // init pixel count
    gfx_v_set_pix_count( hal_pixel_u16_get_pix_count() );

    thread_t_create( gfx_control_thread,
                PSTR("gfx_control"),
                0,
                0 );
}

bool gfx_b_running( void ){

    return TRUE;
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

    int32_t corrected_time_offset = time_offset + ( frame_offset * gfx_u16_get_vm_frame_rate() );

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

    // update_vm_timer();
}

gfx_pixel_array_t master_array;

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
        
    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + FADER_RATE );
        THREAD_WAIT_WHILE( pt,  thread_b_alarm_set() );

        // set master gfx pixel count before starting faders
        // doing this through set_params also runs the error check,
        // updates the dimmer lookup, and updates the master fader.
        gfx_params_t params;
        gfx_v_get_params( &params );
        params.pix_count = hal_pixel_u16_get_pix_count();
        gfx_v_set_params( &params );
        
        uint32_t start = tmr_u32_get_system_time_us();

        gfx_v_process_faders();

        uint32_t elapsed = tmr_u32_elapsed_time_us( start );

        vm_fader_time = elapsed;
    }

PT_END( pt );
}
