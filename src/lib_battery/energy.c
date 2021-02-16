/*
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
 */

#include "sapphire.h"

#include "logging.h"

#include "esp8266.h"
#include "graphics.h"
#include "pixel.h"

#include "energy.h"


#define MICROAMPS_CPU           10000
#define MICROAMPS_WIFI          10000

// pixel calibrations for a single pixel at full power
#define MICROAMPS_RED_PIX       10000
#define MICROAMPS_GREEN_PIX     10000
#define MICROAMPS_BLUE_PIX      10000
#define MICROAMPS_WHITE_PIX     10000

static uint64_t energy_cpu;
static uint64_t energy_wifi;
static uint64_t energy_pix;
static uint64_t energy_total;

KV_SECTION_META kv_meta_t energy_info_kv[] = {
    { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_cpu,        0,    "energy_cpu" },
    { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_wifi,       0,    "energy_wifi" },
    { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_pix,    	 0,    "energy_pix" },
    { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_total,      0,    "energy_total" },
};


PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) );


void energy_v_init( void ){

    thread_t_create( energy_monitor_thread,
                     PSTR("energy_monitor"),
                     0,
                     0 );

}

void energy_v_reset( void ){

	energy_cpu = 0;
	energy_pix = 0;
	energy_wifi = 0;
	energy_total = 0;
}

uint64_t energy_u64_get_total( void ){

    return energy_total;
}

PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( 1 ){

        TMR_WAIT( pt, 100 );

        energy_cpu += MICROAMPS_CPU;

        // if( wifi_b_running() ){

        //     energy_wifi += MICROAMPS_WIFI;    
        // }

        energy_pix += ( gfx_u32_get_pixel_r() * MICROAMPS_RED_PIX ) / 256;
        energy_pix += ( gfx_u32_get_pixel_g() * MICROAMPS_GREEN_PIX ) / 256;
        energy_pix += ( gfx_u32_get_pixel_b() * MICROAMPS_BLUE_PIX ) / 256;
        energy_pix += ( gfx_u32_get_pixel_w() * MICROAMPS_WHITE_PIX ) / 256;
        
        energy_total = 0;
        energy_total += energy_cpu;
        energy_total += energy_wifi;
        energy_total += energy_pix;
    }    

PT_END( pt );
}
