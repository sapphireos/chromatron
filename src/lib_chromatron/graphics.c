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
#include "vm_sync.h"
#include "kvdb.h"
#include "hash.h"
#include "superconductor.h"


static uint16_t vm_fader_time;

KV_SECTION_META kv_meta_t gfx_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_fader_time,        0,                  "vm_fade_time" },
};

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );


void gfx_v_init( void ){

    gfxlib_v_init();

    pixel_v_init();

    #ifdef ENABLE_TIME_SYNC
    vm_sync_v_init();
    #endif

    sc_v_init();

    thread_t_create( gfx_control_thread,
                PSTR("gfx_control"),
                0,
                0 );
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
    pixel_v_signal();

    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + FADER_RATE );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        uint32_t start = tmr_u32_get_system_time_us();

        if( sys_b_is_shutting_down() ){

            gfx_v_shutdown_graphic();
            
            pixel_v_signal();        

            THREAD_EXIT( pt );
        }

        if( vm_b_running() ){

            gfx_v_process_faders();
            gfx_v_sync_array();

            pixel_v_signal();
        }

        uint32_t elapsed = tmr_u32_elapsed_time_us( start );

        vm_fader_time = elapsed;

        // check if LEDs are no longer enabled
        if( !gfx_b_enabled() ){

            // signal the pixel thread.
            // if the pixel driver supports power controls, it
            // will shutdown the IO drivers (so they don't backfeed the pixels)
            pixel_v_signal();        

            THREAD_WAIT_WHILE( pt, !gfx_b_enabled() );

            // reset alarm
            thread_v_set_alarm( tmr_u32_get_system_time_ms() );
        }
    }

PT_END( pt );
}
