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
#include "hal_pixel.h"

void hal_pixel_v_init( void ){

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
