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


static void receive_block( uint8_t data[COPROC_BLOCK_LEN] ){

    coproc_block_t block;
    if( hal_wifi_i8_usart_receive( (uint8_t *)&block, sizeof(block), 10000000 ) < 0 ){

        // timeout, fatal error
        sys_reboot();
    }

    coproc_v_parity_check( &block );

    memcpy( data, block.data, COPROC_BLOCK_LEN );
}

static void send_block( uint8_t data[COPROC_BLOCK_LEN] ){

    coproc_block_t block;
    memcpy( block.data, data, COPROC_BLOCK_LEN );

    coproc_v_parity_generate( &block );

    hal_wifi_v_usart_send_data( (uint8_t *)&block, sizeof(block) );    
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

    // wait for module to boot up
    TMR_WAIT( pt, 2000 );

    hal_wifi_v_usart_flush();

    // wait for sync
    while( hal_wifi_i16_usart_get_char() != COPROC_SYNC );
    // if we don't get a sync, the watchdog timer will restart the entire system.

    // send confirmation
    hal_wifi_v_usart_send_char( COPROC_SYNC );
    
    
    // main message loop
    while(1){

        THREAD_YIELD( pt );

        THREAD_WAIT_WHILE( pt, !hal_wifi_b_usart_rx_available() );

        coproc_hdr_t hdr;
        receive_block( (uint8_t *)&hdr );

        uint8_t buf[COPROC_BUF_SIZE];

        uint8_t n_blocks = 1 + ( hdr.length - 1 ) / 4;
        uint8_t i = 0;
        while( n_blocks > 0 ){
            
            receive_block( &buf[i] );
                
            n_blocks--;

            i += COPROC_BLOCK_LEN;
        }

        uint8_t response[COPROC_BUF_SIZE];
        uint8_t response_len = 0;

        // run command
        coproc_v_dispatch( &hdr, buf, &response_len, response );

        hdr.length = response_len;

        send_block( (uint8_t *)&hdr );

        int16_t data_len = response_len;
        i = 0;
        while( data_len > 0 ){

            send_block( &response[i] );    
            
            i += COPROC_BLOCK_LEN;
            data_len -= COPROC_BLOCK_LEN;
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

