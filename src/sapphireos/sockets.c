/*
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
 */

#include "timers.h"
#include "random.h"
#include "system.h"
#include "sockets.h"
#include "udp.h"

#include "list.h"
#include "memory.h"
#include "netmsg.h"

#include <string.h>

#include "keyvalue.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

#ifdef ENABLE_NETWORK

// raw socket structure
// the beginning of any specific socket structure must
// match this exactly.
typedef struct{
	sock_type_t8 type;          // socket type
	sock_options_t8 options;    // socket options
} sock_state_raw_t;

// datagram structure
typedef struct{
    sock_state_raw_t raw;
    uint8_t state;              // current state
    uint16_t lport;             // local port

    #ifndef SOCK_SINGLE_BUF
    mem_handle_t handle;        // received data handle
    uint8_t header_len;         // length of non-data headers in data handle
    sock_addr_t raddr;          // bound remote address
    #endif

    struct{
        uint8_t setting;
        uint8_t current;
    } timer;                    // receive timeout state
} sock_state_dgram_t;


static list_t sockets;

static uint16_t current_ephemeral_port;

#ifdef SOCK_SINGLE_BUF
static mem_handle_t rx_handle = -1;        // received data handle
static uint16_t rx_port;
static uint8_t rx_header_len;         // length of non-data headers in data handle
static sock_addr_t rx_raddr;          // bound remote address
#endif

bool sock_b_port_in_use( uint16_t port ){

    socket_t sock = sockets.head;

    while( sock >= 0 ){

        sock_state_raw_t *raw_state = list_vp_get_data( sock );

        if( SOCK_IS_DGRAM( raw_state->type ) ){

            sock_state_dgram_t *dgram_state = (sock_state_dgram_t *)raw_state;

            // check port
            if( dgram_state->lport == port ){

                return TRUE;
            }
        }

        sock = list_ln_next( sock );
    }

    return FALSE;
}

bool sock_b_port_busy( uint16_t port ){

    #ifdef SOCK_SINGLE_BUF
    if( rx_handle > 0 ){

        return TRUE;
    }
    #else

    socket_t sock = sockets.head;

    while( sock >= 0 ){

        sock_state_raw_t *raw_state = list_vp_get_data( sock );

        if( SOCK_IS_DGRAM( raw_state->type ) ){

            sock_state_dgram_t *dgram_state = (sock_state_dgram_t *)raw_state;

            // check port
            if( dgram_state->lport != port ){

                goto next;
            }

            // check state
            if( dgram_state->state == SOCK_UDP_STATE_RX_DATA_PENDING ){

                return TRUE;
            }
        }

next:
        sock = list_ln_next( sock );
    }
    #endif

    return FALSE;
}

bool sock_b_rx_pending( void ){

    #ifdef SOCK_SINGLE_BUF
    if( rx_handle > 0 ){

        // check if receiving socket already saw this message

        socket_t sock = sockets.head;
        sock_state_dgram_t *dgram;

        while( sock >= 0 ){

            // derefence to datagram
            dgram = list_vp_get_data( sock );

            // check socket type and port number (if datagram)
            if( ( SOCK_IS_DGRAM( dgram->raw.type ) ) &&
                ( dgram->lport == rx_port ) ){

                // if socket has already seen this message
                if( dgram->state == SOCK_UDP_STATE_RX_DATA_RECEIVED ){

                    // go ahead and release buffer

                    // free the receive buffer
                    mem2_v_free( rx_handle );

                    // mark handle as empty
                    rx_handle = -1;
                    rx_port = 0;

                    return FALSE;
                }                

                break;
            }

            sock = list_ln_next( sock );
        }


        return TRUE;
    }
    #endif

    return FALSE;
}

// this function gets a local port that is between the given bounds and is
// guaranteed to not be in use by any other port.
static uint16_t get_lport( void ){

	do{

		current_ephemeral_port++;

		if( current_ephemeral_port < SOCK_EPHEMERAL_PORT_LOW ){

			current_ephemeral_port = SOCK_EPHEMERAL_PORT_LOW;
		}
		else if( current_ephemeral_port == SOCK_EPHEMERAL_PORT_HIGH ){

            // wraparound to low range
			current_ephemeral_port = SOCK_EPHEMERAL_PORT_LOW;
		}

	} while( sock_b_port_in_use( current_ephemeral_port ) );

	return current_ephemeral_port;
}


// allocate a socket and return its descriptor
// returns -1 if no sockets are available
socket_t sock_s_create( sock_type_t8 type ){

    uint16_t state_size = 0;

    // check state size based on type
    if( type == SOCK_DGRAM ){

        state_size = sizeof(sock_state_dgram_t);
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

    // allocate a memory handle for the socket
    list_node_t ln = list_ln_create_node2( 0, state_size, MEM_TYPE_SOCKET );

    // check if allocation succeeded
    if( ln < 0 ){

        return -1;
    }

    // get raw state
    sock_state_raw_t *socket = list_vp_get_data( ln );

    // set type
    socket->type = type;

    // initialize options
    socket->options = 0;

    // set up type specific state
    if( SOCK_IS_DGRAM( socket->type ) ){

        // upgrade pointer to datagram structure
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)socket;

        dgram->lport            = get_lport();
        dgram->state            = SOCK_UDP_STATE_IDLE;
        dgram->timer.setting    = 0;
        dgram->timer.current    = 0;

        #ifndef SOCK_SINGLE_BUF
        dgram->handle           = -1;
        dgram->header_len       = 0;
        #endif

        netmsg_v_open_close_port( IP_PROTO_UDP, dgram->lport, TRUE );
    }

    // add socket to list
    list_v_insert_tail( &sockets, ln );

    return ln;
}

// release a socket
void sock_v_release( socket_t sock ){

    sock_state_raw_t *s = list_vp_get_data( sock );

    // check type for protocol specific cleanup
    if( SOCK_IS_DGRAM( s->type ) ){

        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        #ifdef SOCK_SINGLE_BUF

        if( ( rx_port == dgram->lport ) && ( rx_handle > 0 ) ){

            mem2_v_free( rx_handle );
            rx_handle = -1;
            rx_port = 0;
        }

        #else
        // check if there is data in the socket's buffer
        if( dgram->handle >= 0 ){

            mem2_v_free( dgram->handle );
        }
        #endif

        netmsg_v_open_close_port( IP_PROTO_UDP, dgram->lport, FALSE );
    }

    // remove socket from list
    list_v_remove( &sockets, sock );

    // release socket
    list_v_release_node( sock );
}


// bind a port to a socket
// generally only a server will need to call this, since on a client the
// port will automatically be assigned.
// This function will assert if the requested port is already in use.
// Note that because of this, it is probably a bad idea to request
// any of the ephemeral ports.
void sock_v_bind( socket_t sock, uint16_t port ){

    sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // assert if port is in use
        ASSERT( sock_b_port_in_use( port ) == FALSE );

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        netmsg_v_open_close_port( IP_PROTO_UDP, dgram->lport, FALSE );

        // set port
        dgram->lport = port;

        netmsg_v_open_close_port( IP_PROTO_UDP, dgram->lport, TRUE );
	}
    else{

        // invalid socket type
        ASSERT( FALSE );
    }
}

void sock_v_set_options( socket_t sock, sock_options_t8 options ){

    sock_state_raw_t *s = list_vp_get_data( sock );

    s->options = options;
}

// timeout is in seconds
void sock_v_set_timeout( socket_t sock, uint8_t timeout ){

	sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        // range check timeout
        if( timeout > SOCK_MAXIMUM_TIMEOUT ){

            timeout = SOCK_MAXIMUM_TIMEOUT;
        }

        // set timer
        dgram->timer.setting = timeout;
        dgram->timer.current = 0;
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }
}

// get the remote address for a socket
void sock_v_get_raddr( socket_t sock, sock_addr_t *raddr ){

	sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        #ifdef SOCK_SINGLE_BUF
        if( dgram->lport == rx_port ){

            *raddr = rx_raddr;
        }
        else{

            memset( raddr, 0, sizeof(sock_addr_t) );
        }
        #else
        *raddr = dgram->raddr;
        #endif
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }
}

// // set the remote address for a socket
// void sock_v_set_raddr( socket_t sock, sock_addr_t *raddr ){

//     sock_state_raw_t *s = list_vp_get_data( sock );

//     if( SOCK_IS_DGRAM( s->type ) ){

//         // get more specific pointer
//         sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

//         dgram->raddr = *raddr;
//     }
//     else{

//         // invalid socket type
//         ASSERT( FALSE );
//     }
// }

uint16_t sock_u16_get_lport( socket_t sock ){

    sock_state_raw_t *s = list_vp_get_data( sock );

    uint16_t port = 0;

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        port = dgram->lport;
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

    return port;
}

int16_t sock_i16_get_bytes_read( socket_t sock ){

    sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        #ifdef SOCK_SINGLE_BUF        
        if( ( dgram->lport == rx_port ) && ( rx_handle > 0 ) ){

            return mem2_u16_get_size( rx_handle ) - rx_header_len;
        }
        #else
        if( dgram->handle > 0 ){

            return mem2_u16_get_size( dgram->handle ) - dgram->header_len;
        }
        #endif
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

    return -1;
}

// Gets a pointer to the data received by the socket.
// This will NOT release the memory!
void *sock_vp_get_data( socket_t sock ){

    // get pointer to socket state
    sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        #ifdef SOCK_SINGLE_BUF

        if( dgram->lport == rx_port ){

            // ensure data has been received for the socket
            ASSERT( rx_handle > 0 );    

            return mem2_vp_get_ptr( rx_handle ) + rx_header_len;
        }

        #else
        // ensure data has been received for the socket
        ASSERT( dgram->handle > 0 );

        return mem2_vp_get_ptr( dgram->handle ) + dgram->header_len;

        #endif
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

    return 0;
}

// Get data handle from socket.
// This will REMOVE the handle from the socket!
// Releasing it will be the caller's responsibility.
mem_handle_t sock_h_get_data_handle( socket_t sock ){

    mem_handle_t h = -1;

    // get pointer to socket state
    sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        #ifdef SOCK_SINGLE_BUF

        if( dgram->lport == rx_port ){

            h = rx_handle;
            rx_handle = -1;
            rx_port = 0;
        }

        #else
        h = dgram->handle;

        dgram->handle = -1;
        #endif
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

    return h;
}

// check if a socket is busy, meaning it will
// not be possible to transmit a message on it.
// note this function does not guarantee that a send
// call will complete successfully, but will give a good
// indication if it will definitely fail.
bool sock_b_busy( socket_t sock ){

    // check memory availability
    if( mem2_u16_get_free() < SOCK_MEM_BUSY_THRESHOLD ){

        return FALSE;
    }

    // the sock parameter is unused, but here because eventually
    // we might want to check other parameters, such as flow control
    // by socket, or source address, routing partners, etc.

    return TRUE;
}

// receive data from a socket.
// returns -1 if socket is busy waiting for data
// returns 0 if data is available.
int8_t sock_i8_recvfrom( socket_t sock ){

	sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        // get more specific pointer
        sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

        // check if idle
        if( dgram->state == SOCK_UDP_STATE_IDLE ){

            // reset timer
            dgram->timer.current = dgram->timer.setting;

            // advance state
            dgram->state = SOCK_UDP_STATE_RX_WAITING;

            return -1;
        }
        // check if waiting
        else if( dgram->state == SOCK_UDP_STATE_RX_WAITING ){

            return -1;
        }
        // check if data has been buffered
        else if( dgram->state == SOCK_UDP_STATE_RX_DATA_PENDING ){

            #ifdef SOCK_SINGLE_BUF
            ASSERT( rx_handle > 0 );
            #else
            ASSERT( dgram->handle > 0 );
            #endif

            // advance state to received
            dgram->state = SOCK_UDP_STATE_RX_DATA_RECEIVED;

            return 0;
        }
        // check if data has been received by the application
        else if( dgram->state == SOCK_UDP_STATE_RX_DATA_RECEIVED ){

            // reset state
            dgram->state = SOCK_UDP_STATE_IDLE;

            #ifdef SOCK_SINGLE_BUF

            if( rx_port == dgram->lport ){

                if( rx_handle > 0 ){

                    mem2_v_free( rx_handle );
                }   

                rx_port = 0;
                rx_handle = -1;
            }

            #else
            // check if socket has memory attached
            if( dgram->handle > 0 ){

                // free the receive buffer
                mem2_v_free( dgram->handle );
            }

            // mark handle as empty
            dgram->handle = -1;

            #endif

            return -1;
        }
        // check if socket was timed out
        else if( dgram->state == SOCK_UDP_STATE_TIMED_OUT ){

            // reset state
            dgram->state = SOCK_UDP_STATE_IDLE;

            return 0;
        }
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

    // socket is busy waiting for data
	return -1;
}

// raw transmit function for a socket.
// NOT a user API
int8_t sock_i8_transmit( socket_t sock, mem_handle_t handle, sock_addr_t *raddr ){

	sock_state_raw_t *s = list_vp_get_data( sock );

    if( SOCK_IS_DGRAM( s->type ) ){

        sock_state_dgram_t *dgram_state = (sock_state_dgram_t *)s;

        uint8_t ttl = 0; // 0 will init to default in IP module

        // check TTL setting
        if( s->options & SOCK_OPTIONS_TTL_1 ){

            // set ttl option
            ttl = 1;
        }

        uint8_t nm_flags = 0;

        // check options
        if( s->options & SOCK_OPTIONS_NO_SECURITY ){

            nm_flags |= NETMSG_FLAGS_WCOM_SECURITY_DISABLE;
        }

        if( s->options & SOCK_OPTIONS_NO_WIRELESS ){

            nm_flags |= NETMSG_FLAGS_NO_WCOM;
        }


        netmsg_t netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

        if( netmsg < 0 ){

            // release data
            mem2_v_free( handle );

            return -1;
        }

        netmsg_state_t *state = netmsg_vp_get_state( netmsg );

        // attach remote address
        if( raddr == 0 ){

            #ifdef SOCK_SINGLE_BUF
            state->raddr = rx_raddr;
            #else
            // get raddr from socket
            state->raddr = dgram_state->raddr;
            #endif
        }
        else{

            state->raddr = *raddr;
        }

        // set local port
        state->laddr.port = dgram_state->lport;

        // attach data handle
        state->data_handle = handle;

        // set ttl
        state->ttl = ttl;

        // set flags
        netmsg_v_set_flags( netmsg, nm_flags );

        if( netmsg_i8_send( netmsg ) < 0 ){

            return -2;
        }
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

	return 0;
}


// send data through a given socket, bufsize bytes from buf, to raddr.
// returns number of bytes written
// for TCP sockets, this function will not work, use send()
// for UDP sockets, bufsize must be less than or equal to the maximum datagram
// size
// raddr contains the remote address to send to.
// if 0 is passed, this will use the auto-bound address in the socket
// returns status based on socket type.
// for UDP, returns 0 if successfully queued, -1 if queueing failed
int16_t sock_i16_sendto( socket_t sock, void *buf, uint16_t bufsize, sock_addr_t *raddr ){

    // allocate data to memory handle
    mem_handle_t h = mem2_h_alloc( bufsize );

    if( h < 0 ){

        return -1;
    }

    memcpy( mem2_vp_get_ptr( h ), buf, bufsize );

    return sock_i16_sendto_m( sock, h, raddr );
}

// send data contained in a memory handle.
// NOTE: this will release the handle!!!
// DO NOT RETAIN A REFERENCE TO THE HANDLE PASSED TO THIS FUNCTION!
int16_t sock_i16_sendto_m( socket_t sock, mem_handle_t handle, sock_addr_t *raddr ){

	sock_state_raw_t *s = list_vp_get_data( sock );

    // check socket type
    if( SOCK_IS_DGRAM( s->type ) ){

        return sock_i8_transmit( sock, handle, raddr );
    }
    else{

        // invalid socket type
        ASSERT( FALSE );
    }

	return 0;
}


// receive a UDP datagram
void sock_v_recv( netmsg_t netmsg ){

    netmsg_state_t *state = netmsg_vp_get_state( netmsg );

    // search for a matching socket
    socket_t sock = sockets.head;
    sock_state_dgram_t *dgram;

    while( sock >= 0 ){

        // derefence to datagram
        dgram = list_vp_get_data( sock );

        // check socket type and port number (if datagram)
        if( ( SOCK_IS_DGRAM( dgram->raw.type ) ) &&
            ( dgram->lport == state->laddr.port ) ){

            break;
        }

        sock = list_ln_next( sock );
    }

    // check if we got a matching socket
    if( sock < 0 ){

        return;
    }

    // if we got here, we have the appropriate socket

    // check if send only
    if( dgram->raw.options & SOCK_OPTIONS_SEND_ONLY ){

        return;
    }

    // check security flags
    if( state->flags & NETMSG_FLAGS_WCOM_SECURITY_DISABLE ){
        // security disabled on this netmsg

        // check if this socket requires secure messages
        if( !( dgram->raw.options & SOCK_OPTIONS_NO_SECURITY ) ){

            // socket requires secure messages

            return;
        }
    }

    #ifdef SOCK_SINGLE_BUF
    // check if the socket is already holding data that has not been
    // received by the owning application.  if so, we'll throw away the
    // incoming data
    if( ( rx_handle > 0 ) && ( rx_port == dgram->lport ) ){

        // so there is buffered data, lets see if the app has
        // retrieved it
        if( dgram->state == SOCK_UDP_STATE_RX_DATA_RECEIVED ){

            // app already saw this, so we'll release it here.

            // free the receive buffer
            mem2_v_free( rx_handle );

            // mark handle as empty
            rx_handle = -1;
            rx_port = 0;
        }
        else{

            // app hasn't received data, so we bail out and this new data
            // gets dropped.

            log_v_debug_P( PSTR("dropped to: %u from %u"), dgram->lport, state->raddr.port );

            return;
        }
    }
    #else
    // check if the socket is already holding data that has not been
    // received by the owning application.  if so, we'll throw away the
    // incoming data
    if( dgram->handle >= 0 ){

        // so there is buffered data, lets see if the app has
        // retrieved it
        if( dgram->state == SOCK_UDP_STATE_RX_DATA_RECEIVED ){

            // app already saw this, so we'll release it here.

            // free the receive buffer
            mem2_v_free( dgram->handle );

            // mark handle as empty
            dgram->handle = -1;
        }
        else{

            // app hasn't received data, so we bail out and this new data
            // gets dropped.

            log_v_debug_P( PSTR("dropped to: %u from %u"), dgram->lport, state->raddr.port );

            return;
        }
    }
    #endif

    #ifdef SOCK_SINGLE_BUF
    
    rx_header_len   = state->header_len;
    rx_handle       = state->data_handle;

    rx_raddr.ipaddr = state->raddr.ipaddr;
    rx_raddr.port   = state->raddr.port;

    rx_port         = dgram->lport;

    #else

    dgram->header_len = state->header_len;

    // assign handle to socket
    dgram->handle = state->data_handle;

    // set remote address
    dgram->raddr.ipaddr = state->raddr.ipaddr;
    dgram->raddr.port   = state->raddr.port;

    #endif

    ASSERT( state->data_handle > 0 );
    
    // remove handle from netmsg
    state->data_handle = -1;


    // set state
    dgram->state = SOCK_UDP_STATE_RX_DATA_PENDING;
}

void sock_v_init( void ){

    list_v_init( &sockets );

    // set a random starting port number
	current_ephemeral_port = SOCK_EPHEMERAL_PORT_LOW + ( rnd_u16_get_int() >> 3 );
}

uint8_t sock_u8_count( void ){

    return list_u8_count( &sockets );
}

void sock_v_process_timeouts( void ){

    // scan through all sockets with timers set
    socket_t sock = sockets.head;

    while( sock >= 0 ){

        sock_state_raw_t *s = list_vp_get_data( sock );

        if( SOCK_IS_DGRAM( s->type ) ){            

            sock_state_dgram_t *dgram = (sock_state_dgram_t *)s;

            // check if timer is active
            if( dgram->timer.current > 0 ){

                // decrement
                dgram->timer.current--;
            }

            // check for timeout
            if( ( dgram->timer.setting > 0 ) &&
                ( dgram->timer.current == 0 ) &&
                ( dgram->state == SOCK_UDP_STATE_RX_WAITING ) ){

                // set state to timed out
                dgram->state = SOCK_UDP_STATE_TIMED_OUT;
            }
        }

        // get next socket
        sock = list_ln_next( sock );
    }
}


#endif
