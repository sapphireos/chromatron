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


void receive( void *buf, uint16_t len ){

    if( hal_wifi_i8_usart_receive( buf, len, 10000000 ) < 0 ){

        // timeout, fatal error
        sys_reboot();
    }
}


PT_THREAD( app_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    hal_wifi_v_init();
    
    wifi_v_start_loader();

    THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

    if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

        log_v_debug_P( PSTR("ESP load failed") );

        THREAD_EXIT( pt );
    }

    hal_wifi_v_enter_normal_mode();


    while(1){

        THREAD_WAIT_WHILE( pt, !hal_wifi_b_usart_rx_available() );

        coproc_block_t block;
        receive( &block, sizeof(block) );

        coproc_hdr_t *hdr = (coproc_hdr_t *)block.data;

        uint8_t buf[COPROC_BUF_SIZE];

        uint8_t n_blocks = 1 + ( hdr->length - 1 ) / 4;
        uint8_t i = 0;
        while( n_blocks > 0 ){
            
            receive( &block, sizeof(block) );
            
            buf[i + 0] = block.data[i + 0];           
            buf[i + 1] = block.data[i + 1];           
            buf[i + 2] = block.data[i + 2];           
            buf[i + 3] = block.data[i + 3];           

            i += 4;
        }
    }

PT_END( pt );	
}



void app_v_init( void ){

    #ifndef ENABLE_WIFI
    thread_t_create( app_thread,
                     PSTR("app"),
                     0,
                     0 );
    #endif
}

