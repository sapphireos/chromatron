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

#include "sapphire.h"

#include "controller.h"
#include "link.h"

static socket_t sock;
static list_t link_list;

typedef struct __attribute__((packed)){
    link2_t link;
    uint8_t timeout;
    ip_addr4_t first_ip;
} link2_meta_t;

static uint8_t count_ip_for_link( link2_meta_t *meta, uint16_t len ){

	ASSERT( len >= sizeof(link2_meta_t) );

	len -= sizeof(link2_meta_t);

	return ( len / sizeof(meta->first_ip) ) + 1;
}

static bool link_mgr_running = FALSE;

PT_THREAD( link2_mgr_server_thread( pt_t *pt, void *state ) );

void link_mgr_v_start( void ){

	if( link_mgr_running ){

		return;
	}

	link_mgr_running = TRUE;

	list_v_init( &link_list );

	thread_t_create( link2_mgr_server_thread,
                 PSTR("link2_mgr_server"),
                 0,
                 0 );
}


list_node_t _link2_mgr_l_lookup_by_hash( uint64_t hash ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link2_meta_t *state = list_vp_get_data( ln );

        if( link2_u64_hash( &state->link ) == hash ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}

list_node_t _link2_mgr_update( link2_t *link, sock_addr_t *raddr ){

	link2_meta_t *meta = 0;
	list_node_t ln = _link2_mgr_l_lookup_by_hash( link2_u64_hash( link ) );

	if( ln < 0 ){

		// link not found
		ln = list_ln_create_node( 0, sizeof(link2_meta_t) );

	    if( ln < 0 ){

	        return -1;
	    }

	    meta = (link2_meta_t *)list_vp_get_data( ln );
	    meta->link = *link;
	    meta->timeout = LINK2_MGR_LINK_TIMEOUT;
	    meta->first_ip = raddr->ipaddr;

	    list_v_insert_tail( &link_list, ln );    

	    log_v_debug_P( PSTR("Add new link: %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );
	}
	

	// link found
	meta = (link2_meta_t *)list_vp_get_data( ln );

	// update IP list
	uint8_t ip_count = count_ip_for_link( meta, list_u16_node_size( ln ) );
	ip_addr4_t *ip = &meta->first_ip;
	bool ip_found = FALSE;

	for( uint8_t i = 0; i < ip_count; i++ ){

		if( ip_b_addr_compare( *ip, raddr->ipaddr ) ){

			// log_v_debug_P( PSTR("found: %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );

			ip_found = TRUE;
			break;
		}

		ip++;
	}

	if( !ip_found ){

		// reallocate and add new IP

		list_node_t new_ln = list_ln_create_node( 0, sizeof(link2_meta_t) + ip_count * sizeof(meta->first_ip) );

		if( new_ln < 0 ){

	        return ln;
	    }

		link2_meta_t *new_meta = (link2_meta_t *)list_vp_get_data( new_ln );

		memcpy( new_meta, meta, list_u16_node_size( ln ) );

		// add IP to end of list
		ip_addr4_t *new_ip = (ip_addr4_t *)( new_meta + 1 ) + ( ip_count - 1 );
		*new_ip = raddr->ipaddr;

		log_v_debug_P( PSTR("Add new IP: %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );

		list_v_insert_tail( &link_list, new_ln );    

		// release old node
		list_v_remove( &link_list, ln );
		list_v_release_node( ln );

		ln = new_ln;
	}	

	// reset timeout
	meta->timeout = LINK2_MGR_LINK_TIMEOUT;


	return ln;
}


PT_THREAD( link2_mgr_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, LINK2_MGR_PORT );

    // sock_v_set_timeout( sock, 1 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            THREAD_EXIT( pt );
        }

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        link2_msg_header_t *header = sock_vp_get_data( sock );

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

        if( header->msg_type == LINK_MSG_TYPE_LINK ){

        	uint8_t count = ( sock_i16_get_bytes_read( sock ) - sizeof(link2_msg_header_t) ) / sizeof(link2_t);

        	// iterate through links
        	link2_t *link = (link2_t *)( header + 1 );

        	while( count > 0 ){

        		_link2_mgr_update( link, &raddr );

        		link++;
        		count--;
        	}
        }

end:
    
        THREAD_YIELD( pt );
    }

PT_END( pt );
}
