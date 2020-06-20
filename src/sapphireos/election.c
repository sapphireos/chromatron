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


#include "sapphire.h"

#include "election.h"

static socket_t sock;

PT_THREAD( election_sender_thread( pt_t *pt, void *state ) );
PT_THREAD( election_server_thread( pt_t *pt, void *state ) );

void election_v_init( void ){

	#if !defined(ENABLE_NETWORK) || !defined(ENABLE_WIFI)
	return;
	#endif

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // create socket
    sock = sock_s_create( SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, ELECTION_PORT );

    sock_v_set_timeout( sock, 1 );

	thread_t_create( election_sender_thread,
                     PSTR("election_sender"),
                     0,
                     0 );

    thread_t_create( election_server_thread,
                     PSTR("election_server"),
                     0,
                     0 );
}


PT_THREAD( election_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		TMR_WAIT( pt, 1000 );

		THREAD_YIELD( pt );
	}    
    
PT_END( pt );
}

PT_THREAD( election_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

		if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        election_msg_t *msg = sock_vp_get_data( sock );

        if( msg->magic != ELECTION_MAGIC ){

        	continue;
        }

        

		THREAD_YIELD( pt );
	}    
    
PT_END( pt );
}



