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

#include "sapphire.h"

#include "logging.h"

#include "graphics.h"
#include "pixel.h"

#include "battery.h"
#include "energy.h"
#include "wifi.h"

#ifdef ENABLE_GFX
// power in microwatts
// static uint32_t power_cpu;
static uint32_t power_pix;
// static uint32_t power_wifi;
// static uint32_t power_total;

// total energy recorded in microwatts per monitor cycle
// static uint64_t energy_cpu;
// static uint64_t energy_wifi;
static uint64_t energy_pix;
// static uint64_t energy_total;

#define ENERGY_MONITOR_RATE FADER_RATE


// convert raw energy counter to milliwatt-hours
static uint64_t convert_to_mwh( uint64_t value ){

    return value / ( 1000 * 3600 * ( 1000 / ENERGY_MONITOR_RATE ) );
}


int8_t energy_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        // energy:
        // convert raw counts to milliwatt-hours

        // if( hash == __KV__energy_cpu ){

        //     STORE64(data, convert_to_mwh( energy_cpu ) );
        // }
        // else if( hash == __KV__energy_wifi ){

        //     STORE64(data, convert_to_mwh( energy_wifi ) );
        // }
        if( hash == __KV__pixel_energy ){

            STORE64(data, convert_to_mwh( energy_pix ) );
        }
        // else if( hash == __KV__energy_total ){

        //     STORE64(data, convert_to_mwh( energy_total ) );
        // }

        // power:
        // convert raw microwatts to millwatts
        // else if( hash == __KV__power_cpu ){

        //     STORE32(data, power_cpu / 1000 );
        // }
        // else if( hash == __KV__power_wifi ){

        //     STORE32(data, power_wifi / 1000 );
        // }
        else if( hash == __KV__pixel_power ){

            STORE32(data, gfx_u32_get_pixel_power() / 1000 );
        }
        // else if( hash == __KV__power_total ){

        //     STORE32(data, power_total / 1000 );
        // }

    }
    else if( op == KV_OP_SET ){

    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}


// runtimes in lifetime total minutes
static uint32_t led_runtime;
static uint32_t sys_runtime;

// totaln milliwatt hours of LED energy
// static uint64_t led_total_energy;


KV_SECTION_META kv_meta_t energy_info_kv[] = {
    // { CATBUS_TYPE_UINT64,   0, 0,  0,                    energy_kv_handler, "energy_cpu" },
    // { CATBUS_TYPE_UINT64,   0, 0,  0,                    energy_kv_handler, "energy_wifi" },
    { CATBUS_TYPE_UINT64,   0, 0,  0,    	             energy_kv_handler, "pixel_energy" },
    // { CATBUS_TYPE_UINT64,   0, 0,  0,                    energy_kv_handler, "energy_total" },

    // { CATBUS_TYPE_UINT32,   0, 0,  0,                    energy_kv_handler, "power_cpu" },
    // { CATBUS_TYPE_UINT32,   0, 0,  0,                    energy_kv_handler, "power_wifi" },
    { CATBUS_TYPE_UINT32,   0, 0,  0,                    energy_kv_handler, "pixel_power" },
    // { CATBUS_TYPE_UINT32,   0, 0,  0,                    energy_kv_handler, "power_total" },

    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY | 
                               KV_FLAGS_PERSIST,    &led_runtime,        0, "pixel_runtime" },
    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY | 
                               KV_FLAGS_PERSIST,    &sys_runtime,        0, "sys_runtime" },
};


PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) );
PT_THREAD( energy_runtime_thread( pt_t *pt, void *state ) );

void energy_v_init( void ){

    thread_t_create( energy_monitor_thread,
                     PSTR("energy_monitor"),
                     0,
                     0 );

    thread_t_create( energy_runtime_thread,
                     PSTR("energy_runtime"),
                     0,
                     0 );

}

void energy_v_reset( void ){

	// energy_cpu = 0;
	energy_pix = 0;
	// energy_wifi = 0;
	// energy_total = 0;
}

uint64_t energy_u64_get_pixel_raw( void ){

    return energy_pix;
}

uint64_t energy_u64_get_pixel_mwh( void ){

    return convert_to_mwh( energy_pix );
}

// uint32_t energy_u32_get_total( void ){

//     return energy_total / ( 3600 * ( 1000 / ENERGY_MONITOR_RATE ) ); // convert to microwatt hours
// }

PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( 1 ){

        TMR_WAIT( pt, ENERGY_MONITOR_RATE );

        // power_wifi = wifi_u32_get_power();
        // power_cpu = cpu_u32_get_power();
        power_pix = gfx_u32_get_pixel_power();

        // energy_cpu += power_cpu;
        // energy_wifi += power_wifi;
        energy_pix += power_pix;

        // power_total = power_pix + power_cpu + power_wifi;
        // energy_total += power_total;
    }    

PT_END( pt );
}


static void flush( void ){

    kv_i8_persist( __KV__power_led_runtime );
    kv_i8_persist( __KV__power_sys_runtime );
}

PT_THREAD( energy_runtime_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint8_t counter;

    counter = 0;

    while( 1 ){

        thread_v_set_alarm( tmr_u32_get_system_time_ms() + 60000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() );

        // if shutting down, flush and terminate thread
        if( sys_b_is_shutting_down() ){

            flush();

            THREAD_EXIT( pt );
        }   

        sys_runtime++;

        if( power_pix > 0 ){

            led_runtime++;
        }


        // flush every 30 minutes
        counter++;

        if( counter == 30 ){

            counter = 0;

            flush();
        }
    }    

PT_END( pt );
}
#endif