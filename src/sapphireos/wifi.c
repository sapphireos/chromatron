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
#include "wifi.h"
#include "timers.h"
#include "ip.h"
#include "threading.h"
#include "logging.h"

#if defined(ENABLE_WIFI) && !defined(AVR)

static ip_addr4_t igmp_groups[WIFI_MAX_IGMP];


PT_THREAD( igmp_thread( pt_t *pt, void *state ) );

void wifi_v_init( void ){

    hal_wifi_v_init();

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( igmp_thread,
                     PSTR("igmp"),
                     0,
                     0 );
   
}

int8_t wifi_i8_igmp_join( ip_addr4_t mcast_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    for( uint8_t i = 0; i < cnt_of_array(igmp_groups); i++ ){

        if( ip_b_addr_compare( mcast_ip, igmp_groups[i] ) ){

            // already joined
            return 0;
        }
    }

    // add new group
    for( uint8_t i = 0; i < cnt_of_array(igmp_groups); i++ ){

        if( ip_b_is_zeroes( igmp_groups[i] ) ){

            igmp_groups[i] = mcast_ip;

            // check if wifi is connected, if so, join here
            // if not, we'll join in the igmp_thread.
            if( wifi_b_connected() ){

                hal_wifi_i8_igmp_join( igmp_groups[i] );   
            }

            break;
        }
    }
    
    return 0;
}

int8_t wifi_i8_igmp_leave( ip_addr4_t mcast_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    for( uint8_t i = 0; i < cnt_of_array(igmp_groups); i++ ){

        if( ip_b_addr_compare( mcast_ip, igmp_groups[i] ) ){

            hal_wifi_i8_igmp_leave( mcast_ip );

            igmp_groups[i] = ip_a_addr(0,0,0,0);

            break;
        }
    }
    
    return 0;
}


PT_THREAD( igmp_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

        TMR_WAIT( pt, 1000 );

        for( uint8_t i = 0; i < cnt_of_array(igmp_groups); i++ ){

            if( ip_b_is_zeroes( igmp_groups[i] ) ){

                continue;
            }

            int8_t status = hal_wifi_i8_igmp_join( igmp_groups[i] );

            if( status != 0 ){

                log_v_error_P( PSTR("IGMP join failed: %d group: %d.%d.%d.%d"), status, igmp_groups[i].ip3, igmp_groups[i].ip2, igmp_groups[i].ip1, igmp_groups[i].ip0 );
            }
        }

        THREAD_WAIT_WHILE( pt, wifi_b_connected() && !sys_b_is_shutting_down() );

        // if shutting down, or lost connection, leave all groups so we'll rejoin later
        if( !wifi_b_connected() || sys_b_is_shutting_down() ){

            for( uint8_t i = 0; i < cnt_of_array(igmp_groups); i++ ){

                if( ip_b_is_zeroes( igmp_groups[i] ) ){

                    continue;
                }

                hal_wifi_i8_igmp_leave( igmp_groups[i] );
            }
        }

        if( sys_b_is_shutting_down() ){

            THREAD_EXIT( pt );
        }
    }

PT_END( pt );
}

#endif