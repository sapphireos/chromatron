// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "config.h"

#include "app.h"
#include "pixel.h"
#include "server.h"
#include "graphics.h"
#include "vm.h"
#include "energy.h"
#include "timesync.h"
#include "automaton.h"
#include "io_kv.h"


SERVICE_SECTION kv_svc_name_t chromatron_service = {"sapphire.device.chromatron"};

// #include "hal_dma.h"

PT_THREAD( dma_test_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // dma_test();

    // socket_t sock = sock_s_create( SOCK_DGRAM );
    // socket_t sock2 = sock_s_create( SOCK_DGRAM );

    // sock_v_bind( sock, 1000 );
    // sock_v_bind( sock2, 1234 );

    // sock_i8_recvfrom( sock2 );

    // sock_addr_t raddr;
    // raddr.ipaddr = ip_a_addr(127,0,0,1);
    // raddr.port = 1234;
    // uint8_t temp = 123;
    // sock_i16_sendto( sock, &temp, sizeof(temp), &raddr );

    // log_v_debug_P( PSTR("%d"), sock_i16_get_bytes_read( sock2 ) );

    // uint8_t *temp2;
    // if( sock_i8_recvfrom( sock2 ) >= 0 ){

    //     temp2 = sock_vp_get_data( sock2 );

    //     log_v_debug_P( PSTR("%u"), *temp2 );
    // }

    // sock_v_release( sock );
    // sock_v_release( sock2 );


    static socket_t sock;

    sock = sock_s_create( SOCK_DGRAM );
    sock_v_bind( sock, 1234 );

    while(1){

       THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

       uint8_t *data = sock_vp_get_data( sock );
       uint16_t len = sock_i16_get_bytes_read( sock );

       // log_v_debug_P( PSTR("rx: %u bytes: 0x%02x, 0x%02x, 0x%02x, 0x%02x"), len, data[0], data[1], data[2], data[3] );

       sock_i16_sendto( sock, data, len, 0 );
    }


PT_END( pt );
}


void app_v_init( void ){

    // thread_t_create( dma_test_thread,
    //                  PSTR("dma_test"),
    //                  0,
    //                  0 );

    iokv_v_init();

    gfx_v_init();

    vm_v_init();

    #ifdef ENABLE_TIME_SYNC
    time_v_init();
    #endif

    #ifdef ENABLE_AUTOMATON
    auto_v_init();
    #endif

    svr_v_init();

    // catbus_query_t query;
    // memset( &query, 0, sizeof(query) );

    // query.tags[0] = __KV__stuff;

    // catbus_l_send( __KV__kv_test_key, __KV__kv_test_key, &query );
    

    // kvdb_i8_add( __KV__test_meow, CATBUS_TYPE_INT16, 1, 0, 0 );
    // kvdb_v_set_name_P( PSTR("test_meow") );

    // query.tags[0] = __KV__stuff;

    // catbus_l_recv( __KV__test_meow, __KV__kv_test_key, &query );
}

