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

#include "system.h"
#include "threading.h"
#include "timers.h"
#include "config.h"
#include "io.h"
#include "flash_fs.h"
#include "wifi.h"
#include "timesync.h"
#include "keyvalue.h"
#include "hal_boards.h"
#include "status_led.h"


/*


Refactor status LED module into a common module with a thin HAL layer here.


Add ability for a button press to disable led quiet mode for 10 seconds.

*/


static bool enabled;

static uint8_t identify;
static uint8_t quiet_mode_override;

KV_SECTION_META kv_meta_t status_led_kv[] = {
    { CATBUS_TYPE_UINT8,         0, 0,   &identify,                  0,   "identify" },
};


static bool is_quiet_mode( void ){

    if( ( cfg_b_get_boolean( CFG_PARAM_ENABLE_LED_QUIET_MODE ) ) &&
        ( tmr_u64_get_system_time_us() > 10000000 ) &&
        ( quiet_mode_override == 0 ) ){

        return TRUE;
    }

    return FALSE;
}


PT_THREAD( status_led_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_WHILE( pt, !enabled );

        if( sys_u32_get_warnings() & SYS_WARN_FLASHFS_FAIL ){

            status_led_v_set( 0, STATUS_LED_RED );

            TMR_WAIT( pt, 1000 );

            status_led_v_set( 1, STATUS_LED_RED );

            TMR_WAIT( pt, 100 );

            status_led_v_set( 0, STATUS_LED_RED );

            TMR_WAIT( pt, 100 );

            status_led_v_set( 1, STATUS_LED_RED );

            TMR_WAIT( pt, 100 );
        }
        else if( cpu_b_osc_fail() ){

            status_led_v_set( 0, STATUS_LED_RED );

            TMR_WAIT( pt, 100 );

            status_led_v_set( 1, STATUS_LED_RED );

            TMR_WAIT( pt, 900 );
        }
        else if( identify > 0 ){

            status_led_v_set( 0, STATUS_LED_WHITE );

            TMR_WAIT( pt, 500 );

            status_led_v_set( 1, STATUS_LED_WHITE );

            TMR_WAIT( pt, 500 );

            identify--;
        }
        else if( sys_u8_get_mode() == SYS_MODE_SAFE ){

            status_led_v_set( 0, STATUS_LED_RED );

            TMR_WAIT( pt, 500 );

            status_led_v_set( 1, STATUS_LED_RED );

            TMR_WAIT( pt, 500 );
        }
        else if( sys_b_is_shutting_down() ){

            status_led_v_set( 0, STATUS_LED_GREEN );

            TMR_WAIT( pt, 200 );

            status_led_v_set( 1, STATUS_LED_GREEN );

            TMR_WAIT( pt, 200 );
        }
        #ifdef ENABLE_WIFI
        else if( wifi_b_connected() ){

            if( wifi_b_ap_mode() ){

                status_led_v_set( 0, STATUS_LED_PURPLE );
            }
            else{

                status_led_v_set( 0, STATUS_LED_BLUE );
            }

            #ifdef ENABLE_TIME_SYNC
            if( time_b_is_sync() ){

                uint32_t net_time = time_u32_get_network_time();

                if( ( ( net_time / LED_TIME_SYNC_INTERVAL ) & 1 ) == 0 ){

                    TMR_WAIT( pt, LED_TIME_SYNC_INTERVAL - ( net_time % LED_TIME_SYNC_INTERVAL ) );
                }
            }
            else{

                TMR_WAIT( pt, 500 );
            }
            #else
            TMR_WAIT( pt, 500 );
            #endif

            if( !is_quiet_mode() ){

                if( wifi_b_ap_mode() ){

                    status_led_v_set( 1, STATUS_LED_PURPLE );
                }
                else{

                    status_led_v_set( 1, STATUS_LED_BLUE );
                }
            }
            #ifdef ENABLE_TIME_SYNC
            if( time_b_is_sync() ){

                uint32_t net_time = time_u32_get_network_time();

                if( ( ( net_time / LED_TIME_SYNC_INTERVAL ) & 1 ) != 0 ){

                    TMR_WAIT( pt, LED_TIME_SYNC_INTERVAL - ( net_time % LED_TIME_SYNC_INTERVAL ) );
                }
            }
            else{

                TMR_WAIT( pt, 500 );
            }
            #else
            TMR_WAIT( pt, 500 );
            #endif
        }
        #endif
        else{

            status_led_v_set( 0, STATUS_LED_GREEN );

            TMR_WAIT( pt, 500 );

            if( !is_quiet_mode() ){

                status_led_v_set( 1, STATUS_LED_GREEN );
            }

            TMR_WAIT( pt, 500 );

            status_led_v_set( 0, STATUS_LED_GREEN );

            TMR_WAIT( pt, 500 );

            if( !is_quiet_mode() ){

                status_led_v_set( 1, STATUS_LED_GREEN );
            }

            TMR_WAIT( pt, 500 );
        }

        if( quiet_mode_override > 0 ){

            quiet_mode_override--;    
        }
    }

PT_END( pt );
}


void status_led_v_init( void ){

    enabled = TRUE;

    
    // HAL INIT
    hal_status_led_v_init();

    thread_t_create( status_led_thread,
                     PSTR("status_led"),
                     0,
                     0 );
}

void status_led_v_override( void ){

    quiet_mode_override = 255;
}

void status_led_v_enable( void ){

    enabled = TRUE;
}

void status_led_v_disable( void ){

    enabled = FALSE;
}

