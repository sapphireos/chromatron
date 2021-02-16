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


#define MICROAMPS_CPU           5000
#define MICROAMPS_WIFI          50000

// pixel calibrations for a single pixel at full power
#define MICROAMPS_RED_PIX       10000
#define MICROAMPS_GREEN_PIX     10000
#define MICROAMPS_BLUE_PIX      10000
#define MICROAMPS_WHITE_PIX     20000

static uint16_t rate_cpu;
static uint16_t rate_wifi;
static uint16_t rate_pix_r;
static uint16_t rate_pix_g;
static uint16_t rate_pix_b;
static uint16_t rate_pix_w;

static uint32_t power_pix;

// total energy recorded
static uint64_t energy_cpu;
static uint64_t energy_wifi;
static uint64_t energy_pix;
static uint64_t energy_total;


int8_t energy_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        // convert raw counts to microamp-hours

        if( hash == __KV__energy_cpu ){

            STORE64(data, energy_cpu / ( 3600 * ( 1000 / FADER_RATE ) ) );
        }
        else if( hash == __KV__energy_wifi ){

            STORE64(data, energy_wifi / ( 3600 * ( 1000 / FADER_RATE ) ) );
        }
        else if( hash == __KV__energy_pix ){

            STORE64(data, energy_pix / ( 3600 * ( 1000 / FADER_RATE ) ) );
        }
        else if( hash == __KV__energy_total ){

            STORE64(data, energy_total / ( 3600 * ( 1000 / FADER_RATE ) ) );
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
    { SAPPHIRE_TYPE_UINT64,   0, 0,  0,             energy_kv_handler,    "energy_cpu" },
    { SAPPHIRE_TYPE_UINT64,   0, 0,  0,             energy_kv_handler,    "energy_wifi" },
    { SAPPHIRE_TYPE_UINT64,   0, 0,  0,    	        energy_kv_handler,    "energy_pix" },
    { SAPPHIRE_TYPE_UINT32,   0, 0,  &power_pix,    0,                    "energy_pix_power" },
    { SAPPHIRE_TYPE_UINT64,   0, 0,  0,             energy_kv_handler,    "energy_total" },
};


PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) );


void energy_v_init( void ){

    rate_cpu    = MICROAMPS_CPU;
    rate_wifi   = MICROAMPS_WIFI;
    rate_pix_r  = MICROAMPS_RED_PIX;
    rate_pix_g  = MICROAMPS_GREEN_PIX;
    rate_pix_b  = MICROAMPS_BLUE_PIX;
    rate_pix_w  = MICROAMPS_WHITE_PIX;

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

    return energy_total / ( 3600 * ( 1000 / FADER_RATE ) );
}

PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( 1 ){

        TMR_WAIT( pt, FADER_RATE );

        energy_cpu += rate_cpu;
        energy_wifi += rate_wifi;

        power_pix = ( gfx_u32_get_pixel_r() * rate_pix_r ) / 256;
        power_pix += ( gfx_u32_get_pixel_g() * rate_pix_g ) / 256;
        power_pix += ( gfx_u32_get_pixel_b() * rate_pix_b ) / 256;
        power_pix += ( gfx_u32_get_pixel_w() * rate_pix_w ) / 256;
            
        energy_pix += power_pix;

        energy_total = 0;
        energy_total += energy_cpu;
        energy_total += energy_wifi;
        energy_total += energy_pix;
    }    

PT_END( pt );
}
