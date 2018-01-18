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

#include "graphics.h"
#include "pixel.h"

#include "vm.h"
#include "server.h"


static socket_t sock;


PT_THREAD( server_thread( pt_t *pt, void *state ) );



void svr_v_init( void ){

    sock = sock_s_create( SOCK_DGRAM );

    sock_v_bind( sock, CHROMATRON_SERVER_PORT );

    // set timeout
    sock_v_set_timeout( sock, CHROMA_SERVER_TIMEOUT );


    // start server
    thread_t_create( server_thread,
                     PSTR("chroma_server"),
                     0,
                     0 );
}


PT_THREAD( server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		// listen
		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check for timeout
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            if( !gfx_b_running() ){
                
                // re-enable pixel bridge and reset VM
                gfx_v_pixel_bridge_enable();            
                vm_v_reset();
            }

            continue;
        }

        uint8_t *data = sock_vp_get_data( sock );
        uint8_t type = *data;

        if( ( type == CHROMA_MSG_TYPE_HSV ) ||
            ( type == CHROMA_MSG_TYPE_RGB ) ){

            // stop pixel bridge if loading pixels directly
            gfx_v_pixel_bridge_disable();

            chroma_msg_pixel_t *msg = (chroma_msg_pixel_t *)data;

            // bounds check
            if( msg->count > CHROMA_SVR_MAX_PIXELS ){

                continue;
            }

            uint8_t array_r[CHROMA_SVR_MAX_PIXELS];
            uint8_t array_g[CHROMA_SVR_MAX_PIXELS];
            uint8_t array_b[CHROMA_SVR_MAX_PIXELS];
            uint8_t array_d[CHROMA_SVR_MAX_PIXELS];
                
            // check if HSV
            if( type == CHROMA_MSG_TYPE_HSV ){

                // unpack HSV pointers
                uint16_t *h = &msg->data0;
                uint16_t *s = h + 1;
                uint16_t *v = s + 1;

                // convert to RGB
                for( uint8_t i = 0; i < msg->count; i++ ){

                    // update in place
                    gfx_v_hsv_to_rgb( *h, *s, *v, h, s, v );

                    h += 3;
                    s += 3;
                    v += 3;
                }
            }

            // unpack RGB pointers
            uint16_t *r = &msg->data0;
            uint16_t *g = r + 1;
            uint16_t *b = g + 1;
            uint8_t dither;

            for( uint8_t i = 0; i < msg->count; i++ ){

                *r /= 64;
                *g /= 64;
                *b /= 64;

                dither =  ( *r & 0x0003 ) << 4;
                dither |= ( *g & 0x0003 ) << 2;
                dither |= ( *b & 0x0003 );

                *r /= 4;
                *g /= 4;
                *b /= 4;
            
                array_r[i] = *r;
                array_g[i] = *g;
                array_b[i] = *b;
                array_d[i] = dither;

                r += 3;
                g += 3;
                b += 3;
            }

            pixel_v_load_rgb( 
                msg->index, 
                msg->count, 
                array_r, 
                array_g, 
                array_b, 
                array_d );
        }
    }

PT_END( pt );
}
