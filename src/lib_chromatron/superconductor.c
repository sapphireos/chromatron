/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "superconductor.h"

#ifdef ENABLE_SERVICES

KV_SECTION_META kv_meta_t superconductor_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,   0, KV_FLAGS_PERSIST,  0, 		0, "sc_enable" },
};


static socket_t sock;


PT_THREAD( superconductor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	// create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    if( sock < 0 ){

		log_v_critical_P( PSTR("socket fail") );

		THREAD_EXIT( pt );    	
    }

	log_v_info_P( PSTR("SuperConductor server is running") );
  
	services_v_listen( __KV__SuperConductor, 0 );

	while( TRUE ){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }


	}

PT_END( pt );
}


#endif

void sc_v_init( void ){

	#ifdef ENABLE_SERVICES

	bool enable = FALSE;

	kv_i8_get( __KV__sc_enable, &enable, sizeof(enable) );

	if( !enable ){

		return;
	}

	thread_t_create( superconductor_thread,
                PSTR("superconductor"),
                0,
                0 );
	#else

	#endif
}

