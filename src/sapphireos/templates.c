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


#if 0 // don't compile this module!

// "one-shot" thread
PT_THREAD( one_shot_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );  
    
    // do stuff...
    
PT_END( pt );
}

// daemon thread
PT_THREAD( daemon_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );  
    
    while(1){
        
        // stuff goes here...

        THREAD_YIELD( pt );
    }

PT_END( pt );
}

// infinite server thread
PT_THREAD( server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );  
    
    // create socket
	static socket_t sock;
    sock = sock_s_create( SOS_SOCK_DGRAM );
    
    // assert if socket was not created
    ASSERT( sock >= 0 );

	// bind to port
	sock_v_bind( sock, PORT );

    while(1){
            
		// wait for a datagram
        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );
        
		// get data
		uint8_t *type = sock_vp_get_data( sock );
        
        // process data

        
    }

PT_END( pt );
}


// list iteration:
{

    list_node_t ln = list.head;

    while( ln >= 0 ){
        
        // get state for current list node
        void *state = list_vp_get_data( ln );
        

        ln = list_ln_next( ln );
    }

}

#endif

