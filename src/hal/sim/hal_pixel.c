/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
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
 */

#include "cpu.h"
#include "system.h"
#include "timers.h"
#include "logging.h"
#include "threading.h"
#include "hal_pixel.h"
#include "pixel.h"
#include "hal_sim_sdl.h"

// graphics.c kicks the pixel driver with PIX_SIGNAL_0 when the array comes
// up (pixel_v_signal()), and on hardware a driver thread consumes that
// signal (see hal_pixel.c for esp32/esp8266).  Nothing consumed it here, so
// the signal bit stayed raised forever: the scheduler's signalled-thread
// pass never settled, and the sleep condition in thread_core() - no
// signals pending - was never true.
PT_THREAD( sim_pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( 1 ){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

        // there is no pixel hardware to start: transfers are synchronous,
        // in hal_pixel_v_start_transfer()

        // --sdl: draw the frame that just went out to the strips
        sim_sdl_v_frame();
    }

PT_END( pt );
}

void hal_pixel_v_init( void ){

    thread_t_create( THREAD_CAST(sim_pixel_thread), PSTR("sim_pixel"), 0, 0 );
}

void hal_pixel_v_configure( void ){

}


static uint32_t sim_transfer_count = 0;
static uint32_t sim_last_log_ms = 0;

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){

    // the simulator has no pixel hardware: count the transfers and emit
    // a status line about once a second so the VM -> framebuffer ->
    // pixel output path is visibly running
    sim_transfer_count++;

    uint32_t now = tmr_u32_get_system_time_ms();
    if( now - sim_last_log_ms > 1000 ){

        sim_last_log_ms = now;
        log_v_info_P( PSTR( "sim pixel: %d bytes transferred (%d transfers)" ), len, sim_transfer_count );
    }
}
