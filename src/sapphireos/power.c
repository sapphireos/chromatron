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
#include "hal_cpu.h"

#include "power.h"
#include "threading.h"
#include "keyvalue.h"
#include "timers.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"


#ifdef ENABLE_POWER

// static bool doze_mode;

KV_SECTION_META kv_meta_t power_kv[] = {
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,   0, 0,                         "enable_cpu_low_power" },
    // { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,   0, 0,                      "enable_low_power" },
    // { CATBUS_TYPE_BOOL,     0, KV_FLAGS_READ_ONLY, &doze_mode, 0,             "power_doze_mode" },
};


// PT_THREAD( doze_thread( pt_t *pt, void *state ) );

void pwr_v_init( void ){

    if( kv_b_get_boolean( __KV__enable_cpu_low_power ) ){

        cpu_v_set_clock_speed_low();
    }
    else{

        cpu_v_set_clock_speed_high();   
    }

    // if( !kv_b_get_boolean( __KV__enable_low_power ) ){

    //     return;
    // }

    // thread_t_create( doze_thread,
    //                 PSTR("dozer"),
    //                 0,
    //                 0 );    
}


void pwr_v_sleep( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }    

    // set sleep mode
    cpu_v_sleep();
}

void pwr_v_wake( void ){

}

bool pwr_b_is_doze_mode( void ){

    // return doze_mode;
    return FALSE;
}


// PT_THREAD( doze_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

//     while( TRUE ){

//         TMR_WAIT( pt, 4000 );

//         if( !doze_mode ){

//             // not in doze mode

//             // check gfx sync state
//             uint8_t sync_state = 0;
//             kv_i8_get( __KV__gfx_sync_state, &sync_state, sizeof(sync_state) );

//             if( sync_state == 0 ){

//                 log_v_debug_P( PSTR("Entering doze mode") );

//                 doze_mode = TRUE;
//             }

//             // if changing into doze mode:
//             if( doze_mode ){

//                 // replace the KV sets with direct driver support in
//                 // hal_wifi (after confirming this concept works).
                    
//                 bool enable_modem_sleep = TRUE;

//                 if( kv_i8_set( __KV__wifi_enable_modem_sleep, &enable_modem_sleep, sizeof(enable_modem_sleep) ) != KV_ERR_STATUS_OK ){


//                 }
//             }
//         }
//         else{

//             // doze mode

//             // check gfx sync state
//             uint8_t sync_state = 0;
//             kv_i8_get( __KV__gfx_sync_state, &sync_state, sizeof(sync_state) );

//             if( sync_state != 0 ){

//                 log_v_debug_P( PSTR("Leaving doze mode") );

//                 doze_mode = FALSE;
//             }

//             // leaving doze mode
//             if( !doze_mode ){

//                 bool enable_modem_sleep = FALSE;

//                 kv_i8_set( __KV__wifi_enable_modem_sleep, &enable_modem_sleep, sizeof(enable_modem_sleep) );
//             }
//         }   
//     }

// PT_END( pt );
// }



#endif
