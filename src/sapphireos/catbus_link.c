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
#include "timers.h"
#include "sockets.h"
#include "threading.h"
#include "fs.h"
#include "keyvalue.h"
#include "catbus.h"

#include "catbus_link.h"

#ifdef ENABLE_CATBUS_LINK

PT_THREAD( link_server_thread( pt_t *pt, void *state ) );

static socket_t sock;

void link_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

    thread_t_create( link_server_thread,
                 PSTR("link_server"),
                 0,
                 0 );
}

void link_v_handle_shutdown( ip_addr4_t ip ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

}

void link_v_shutdown( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

}


PT_THREAD( link_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, LINK_PORT );

    // sock_v_set_timeout( sock, 1 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        link_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->magic != LINK_MAGIC ){

            goto end;
        }

        if( header->version != LINK_VERSION ){

            goto end;
        }

        // filter our own messages
        if( header->origin_id == catbus_u64_get_origin_id() ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        // log_v_debug_P( PSTR("%d"), header->msg_type );

        // if( header->msg_type == CATBUS_MSG_TYPE_ANNOUNCE ){

            // no op
        // }

        // const catbus_hash_t32 *tags = catbus_hp_get_tag_hashes();
 

end:
    
        THREAD_YIELD( pt );
    }

PT_END( pt );
}




#else

void link_v_init( void ){
    
}

#endif
