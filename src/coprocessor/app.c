// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#include "sapphire.h"

#include "coprocessor.h"

#include "esp8266_loader.h"
#include "hal_esp8266.h"

#include "app.h"



PT_THREAD( app_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    // hal_wifi_v_init();
    
    // TMR_WAIT( pt, 5000 );

    // wifi_v_start_loader();

    // THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

    // if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

    //     THREAD_EXIT( pt );
    // }


PT_END( pt );	
}



void app_v_init( void ){

    thread_t_create( app_thread,
                     PSTR("app"),
                     0,
                     0 );
}

