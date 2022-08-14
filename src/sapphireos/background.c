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

#include "msgflow.h"
#include "background.h"

PT_THREAD( background_thread( pt_t *pt, void *state ) );

void background_v_init( void ){

    thread_t_create( background_thread, PSTR("background"), 0, 0 );
}


PT_THREAD( background_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        sys_v_wdt_reset();
        
        #ifdef ENABLE_NETWORK
        sock_v_process_timeouts();
        #endif

        #ifdef ENABLE_MSGFLOW
        msgflow_v_process_timeouts();
        #endif
    }

PT_END( pt );
}

