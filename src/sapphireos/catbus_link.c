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
#include "list.h"
#include "hash.h"

#include "catbus_link.h"

#ifdef ENABLE_CATBUS_LINK


PT_THREAD( link_server_thread( pt_t *pt, void *state ) );

static list_t link_list;
static socket_t sock;

void link_v_init( void ){

	list_v_init( &link_list );

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

    thread_t_create( link_server_thread,
                 PSTR("link_server"),
                 0,
                 0 );
}

link_state_t link_ls_assemble(
	link_mode_t8 mode, 
    catbus_hash_t32 source_hash, 
    catbus_hash_t32 dest_hash, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter ){

	link_state_t state = {
		.mode 				= mode,
		.source_hash 		= source_hash,
		.dest_hash 			= dest_hash,
		.query 				= *query,
		.tag 				= tag,
		.rate 			    = rate,
		.aggregation 		= aggregation,
		.filter 			= filter,
	};

	return state;
}

link_state_t* link_ls_get_data( link_handle_t link ){

    return list_vp_get_data( link );    
}

bool link_b_compare( link_state_t *link1, link_state_t *link2 ){
 
	return memcmp( link1, link2, sizeof(link_state_t) ) == 0;
}

uint64_t link_u64_hash( link_state_t *link ){

	return hash_u64_data( (uint8_t *)link, sizeof(link_state_t) );	
}

link_handle_t link_l_lookup( link_state_t *link ){

	list_node_t ln = link_list.head;

	while( ln >= 0 ){

		link_state_t *state = list_vp_get_data( ln );

		if( link_b_compare( link, state ) ){

			return ln;
		}

		ln = list_ln_next( ln );
	}

	return -1;
}

link_handle_t link_l_lookup_by_hash( uint64_t hash ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link_state_t *state = list_vp_get_data( ln );

        if( link_u64_hash( state ) == hash ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}

link_handle_t link_l_create( 
    link_mode_t8 mode, 
    catbus_hash_t32 source_hash, 
    catbus_hash_t32 dest_hash, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter ){ 

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return -1;
	}

	ASSERT( tag != 0 );

	link_state_t state = link_ls_assemble(
							mode,
							source_hash,
							dest_hash,
							query,
							tag,
							rate,
							aggregation,
							filter );

    return link_l_create2( &state );
}


link_handle_t link_l_create2( link_state_t *state ){ 

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return -1;
    }

    ASSERT( state->tag != 0 );

    link_handle_t lh = link_l_lookup( state );

    if( lh > 0 ){

        return lh;
    }

    if( list_u8_count( &link_list ) >= LINK_MAX_LINKS ){

        return -1;
    }

    list_node_t ln = list_ln_create_node2( state, sizeof(link_state_t), MEM_TYPE_CATBUS_LINK );

    if( ln < 0 ){

        return -1;
    }

    list_v_insert_tail( &link_list, ln );

    return ln;
}

void link_v_delete_by_tag( catbus_hash_t32 tag ){
	
	list_node_t ln = link_list.head;

	while( ln >= 0 ){

		link_state_t *state = list_vp_get_data( ln );
		list_node_t next_ln = list_ln_next( ln );

		if( state->tag == tag ){

			list_v_remove( &link_list, ln );
			list_v_release_node( ln );
		}

		ln = next_ln;
	}
}

void link_v_delete_by_hash( uint64_t hash ){
    
    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link_state_t *state = list_vp_get_data( ln );
        list_node_t next_ln = list_ln_next( ln );

        if( link_u64_hash( state ) == hash ){

            list_v_remove( &link_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }
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

        link_msg_header_t *header = sock_vp_get_data( sock );

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


#endif
