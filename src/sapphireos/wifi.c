/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

        if( ip_b_is_zeroes( igmp_groups[i] ) ){

            igmp_groups[i] = mcast_ip;

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

        TMR_WAIT( pt, 500 );

        for( uint8_t i = 0; i < cnt_of_array(igmp_groups); i++ ){

            if( ip_b_is_zeroes( igmp_groups[i] ) ){

                continue;
            }

            int8_t status = hal_wifi_i8_igmp_join( igmp_groups[i] );

            if( status != 0 ){

                log_v_error_P( PSTR("IGMP join failed: %d"), status );
            }
        }

        THREAD_WAIT_WHILE( pt, wifi_b_connected() );

        TMR_WAIT( pt, 1000 );
    }

PT_END( pt );
}
