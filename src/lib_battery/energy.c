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

#include "battery.h"
#include "energy.h"
#include "wifi.h"

static uint32_t power_cpu;
static uint32_t power_wifi;
static uint32_t power_total;

// total energy recorded
static uint64_t energy_cpu;
static uint64_t energy_wifi;
static uint64_t energy_pix;
static uint64_t energy_total;

#define ENERGY_MONITOR_RATE FADER_RATE


int8_t energy_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        // energy:
        // convert raw counts to milliwatt-hours

        if( hash == __KV__energy_cpu ){

            STORE64(data, energy_cpu / ( 1000 * 3600 * ( 1000 / ENERGY_MONITOR_RATE ) ) );
        }
        else if( hash == __KV__energy_wifi ){

            STORE64(data, energy_wifi / ( 1000 * 3600 * ( 1000 / ENERGY_MONITOR_RATE ) ) );
        }
        else if( hash == __KV__energy_pix ){

            STORE64(data, energy_pix / ( 1000 * 3600 * ( 1000 / ENERGY_MONITOR_RATE ) ) );
        }
        else if( hash == __KV__energy_total ){

            STORE64(data, energy_total / ( 1000 * 3600 * ( 1000 / ENERGY_MONITOR_RATE ) ) );
        }

        // power:
        // convert raw microwatts to millwatts
        else if( hash == __KV__power_cpu ){

            STORE32(data, power_cpu / 1000 );
        }
        else if( hash == __KV__power_wifi ){

            STORE32(data, power_wifi / 1000 );
        }
        else if( hash == __KV__power_pix ){

            uint32_t power_pix = gfx_u32_get_pixel_power();

            STORE32(data, power_pix / 1000 );
        }
        else if( hash == __KV__power_total ){

            STORE32(data, power_total / 1000 );
        }

    }
    else if( op == KV_OP_SET ){

    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}


KV_SECTION_META kv_meta_t energy_info_kv[] = {
    { CATBUS_TYPE_UINT64,   0, 0,  0,             energy_kv_handler,    "energy_cpu" },
    { CATBUS_TYPE_UINT64,   0, 0,  0,             energy_kv_handler,    "energy_wifi" },
    { CATBUS_TYPE_UINT64,   0, 0,  0,    	      energy_kv_handler,    "energy_pix" },
    { CATBUS_TYPE_UINT64,   0, 0,  0,             energy_kv_handler,    "energy_total" },

    { CATBUS_TYPE_UINT32,   0, 0,  0,             energy_kv_handler,    "power_cpu" },
    { CATBUS_TYPE_UINT32,   0, 0,  0,             energy_kv_handler,    "power_wifi" },
    { CATBUS_TYPE_UINT32,   0, 0,  0,             energy_kv_handler,    "power_pix" },
    { CATBUS_TYPE_UINT32,   0, 0,  0,             energy_kv_handler,    "power_total" },
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

uint32_t energy_u32_get_total( void ){

    return energy_total / ( 3600 * ( 1000 / ENERGY_MONITOR_RATE ) ); // convert to microwatt hours
}

PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( 1 ){

        TMR_WAIT( pt, ENERGY_MONITOR_RATE );

        power_wifi = wifi_u32_get_power();
        power_cpu = cpu_u32_get_power();
        uint32_t power_pix = gfx_u32_get_pixel_power();

        energy_cpu += power_cpu;
        energy_wifi += power_wifi;
        energy_pix += power_pix;

        power_total = power_pix + power_cpu + power_wifi;
        energy_total += power_total;
    }    

PT_END( pt );
}
