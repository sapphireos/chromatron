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
#include "hal_pixel.h"
#include "graphics.h"
#include "vm.h"
#include "timesync.h"
#include "kvdb.h"
#include "hash.h"

#ifdef ENABLE_TIME_SYNC
static uint16_t vm0_frame_number;
static uint32_t vm0_frame_ts;
static bool update_frame_rate;
#endif

static uint16_t vm_fader_time;

KV_SECTION_META kv_meta_t gfx_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_fader_time,        0,                  "vm_fade_time" },
};

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) );


void gfx_v_init( void ){

    gfxlib_v_init();

    pixel_v_init();

    thread_t_create( gfx_control_thread,
                PSTR("gfx_control"),
                0,
                0 );
}

bool gfx_b_running( void ){

    return TRUE;
}

void gfx_v_set_sync0( uint16_t frame, uint32_t ts ){

    #ifdef ENABLE_TIME_SYNC
    vm0_frame_number = frame;
    vm0_frame_ts = ts;

    update_frame_rate = TRUE;
    #endif
}

PT_THREAD( gfx_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
        
    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + FADER_RATE );
        THREAD_WAIT_WHILE( pt,  thread_b_alarm_set() );

        uint32_t start = tmr_u32_get_system_time_us();

        gfx_v_process_faders();

        uint32_t elapsed = tmr_u32_elapsed_time_us( start );

        vm_fader_time = elapsed;
    }

PT_END( pt );
}
