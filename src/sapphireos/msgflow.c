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
#include "sockets.h"
#include "netmsg.h"
#include "timers.h"
#include "threading.h"
#include "keyvalue.h"
#include "random.h"
#include "list.h"
#include "msgflow.h"

#include "logging.h"

#ifdef ENABLE_MSGFLOW

typedef struct{
    catbus_hash_t32 service;
    uint16_t sequence;
    socket_t sock;
    sock_addr_t raddr;
    uint8_t code;
    uint16_t max_msg_size;
    bool shutdown;
    list_node_t ln;
    uint8_t timeout;
} msgflow_state_t;


PT_THREAD( listener_thread( pt_t *pt, void *state ) );
PT_THREAD( msgflow_thread( pt_t *pt, msgflow_state_t *state ) );

static list_t msgflow_list;
static socket_t listener_sock = -1;

void msgflow_v_init( void ){

    list_v_init( &msgflow_list );
}

msgflow_t msgflow_m_listen( catbus_hash_t32 service, uint8_t code, uint16_t max_msg_size ){

    // check if listener is running
    if( listener_sock < 0 ){

        // start it up
        if( thread_t_create( listener_thread,
                             PSTR("msgflow_listener"),
                             0,
                             0 ) > 0 ){

            // set sock to 0.
            // this is a bit of a trick.
            // 0 is always invalid on memory handles, but alloc failures will always return
            // negative.  we init to -1, then set to 0 here creating the thread.
            // it's still an invalid socket, but if we call listen() again before the thread
            // has a chance to init the socket, we won't accidentally create a second listener
            // thread.
            listener_sock = 0;
        }
    }

    msgflow_state_t state = {0};
    state.service       = service;
    state.sock          = sock_s_create( SOCK_DGRAM );
    state.code          = code;
    state.max_msg_size  = max_msg_size;
    state.shutdown      = FALSE;

    thread_t t = 
        thread_t_create( THREAD_CAST(msgflow_thread),
                         PSTR("msgflow"),
                         &state,
                         sizeof(state) );

    msgflow_t msgflow = t;

    list_node_t ln = list_ln_create_node( &t, sizeof(t) );

    if( ( state.sock < 0 ) || ( t < 0 ) || ( ln < 0 ) ){

        if( state.sock > 0 ){

            sock_v_release( state.sock );
        }

        if( t > 0 ){

            thread_v_kill( t );
        }

        if( ln > 0 ){

            list_v_release_node( ln );
        }

        return -1;
    }

    list_v_insert_head( &msgflow_list, ln );

    msgflow_state_t *mstate = thread_vp_get_data( msgflow );
    mstate->ln = ln;

    return msgflow;
}

bool msgflow_b_connected( msgflow_t msgflow ){

    msgflow_state_t *state = thread_vp_get_data( msgflow );

    return !ip_b_is_zeroes( state->raddr.ipaddr );
}

bool msgflow_b_send( msgflow_t msgflow, void *data, uint16_t len ){

    return TRUE;
}

void msgflow_v_close( msgflow_t msgflow ){

    msgflow_state_t *state = thread_vp_get_data( msgflow );

    state->shutdown = TRUE;
}

static bool validate_header( msgflow_header_t *header ){

    if( ( header->flags & MSGFLOW_FLAGS_VERSION_MASK ) != MSGFLOW_FLAGS_VERSION ){

        return FALSE;
    }

    return TRUE;
}

static bool send_msg( msgflow_state_t *state, uint8_t type, void *data, uint16_t len ){

    // check if we have a remote address to send to
    if( ip_b_is_zeroes( state->raddr.ipaddr ) ){

        return FALSE;
    }

    uint16_t mem_len = len + sizeof(msgflow_header_t);

    mem_handle_t h = mem2_h_alloc( mem_len );

    if( h < 0 ){

        return FALSE;
    }

    void *ptr = mem2_vp_get_ptr_fast( h );

    msgflow_header_t header = {
        MSGFLOW_FLAGS_VERSION,
        type
    };

    memcpy( ptr, &header, sizeof(header) );
    ptr += sizeof(header);
    memcpy( ptr, data, len );

    if( sock_i16_sendto_m( state->sock, h, &state->raddr ) < 0 ){

        return FALSE;
    }

    return TRUE;
}

static void send_reset( msgflow_state_t *state ){

    msgflow_msg_reset_t reset = {
        state->sequence,            
        state->code,
        0
    };

    if( !send_msg( state, MSGFLOW_TYPE_RESET, &reset, sizeof(reset) ) ){

    }
}

static void send_stop( msgflow_state_t *state ){

    if( !send_msg( state, MSGFLOW_TYPE_STOP, 0, 0 ) ){

    }
}

void msgflow_v_process_timeouts( void ){

    list_node_t ln = msgflow_list.head;

    while( ln > 0 ){

        msgflow_t *m = (msgflow_t *)list_vp_get_data( ln );
        msgflow_state_t *mstate = thread_vp_get_data( *m );

        // check if address is valid (and thread isn't shutting down)
        if( !ip_b_is_zeroes( mstate->raddr.ipaddr ) && !mstate->shutdown ){

            // check if timeout is expired
            if( mstate->timeout > 0 ){

                mstate->timeout--;
            }

            // check for timeout
            if( mstate->timeout == 0 ){

                log_v_debug_P( PSTR("msgflow timed out") );

                // restart the thread
                thread_v_restart( *m );
            }
        }

        ln = list_ln_next( ln );
    }
}


PT_THREAD( listener_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    listener_sock = sock_s_create( SOCK_DGRAM );

    if( listener_sock < 0 ){

        THREAD_EXIT( pt );
    }

    while(1){

        // listen for sink
        THREAD_WAIT_WHILE( pt, sock_i16_get_bytes_read( listener_sock ) < 0 );

        // get message
        msgflow_header_t *header = sock_vp_get_data( listener_sock );

        if( !validate_header( header ) ){

            continue;
        }

        if( header->type != MSGFLOW_TYPE_SINK ){

            continue;
        }

        msgflow_msg_sink_t *sink = (msgflow_msg_sink_t *)( header + 1 );

        // search for service
        list_node_t ln = msgflow_list.head;

        while( ln > 0 ){

            msgflow_t *m = (msgflow_t *)list_vp_get_data( ln );
            msgflow_state_t *mstate = thread_vp_get_data( *m );

            if( mstate->service == sink->service ){

                // check if flow hasn't been initialized
                if( ip_b_is_zeroes( mstate->raddr.ipaddr ) ){

                    // reset timeout
                    mstate->timeout = MSGFLOW_TIMEOUT;

                    // set address
                    sock_v_get_raddr( listener_sock, &mstate->raddr );
                
                    log_v_debug_P( PSTR("got sink %d.%d.%d.%d"), mstate->raddr.ipaddr.ip3, mstate->raddr.ipaddr.ip2, mstate->raddr.ipaddr.ip1, mstate->raddr.ipaddr.ip0 );
                }

                // we don't break here.
                // we might have multiple msgflows running on the same service.
            }

            ln = list_ln_next( ln );
        }
    }
    
PT_END( pt );
}

PT_THREAD( msgflow_thread( pt_t *pt, msgflow_state_t *state ) )
{
PT_BEGIN( pt );
    
    sock_v_set_timeout( state->sock, 1 );

    while( !state->shutdown ){

        memset( &state->raddr, 0, sizeof(state->raddr) );

        THREAD_WAIT_WHILE( pt, ip_b_is_zeroes( state->raddr.ipaddr ) && !state->shutdown );

        if( state->shutdown ){

            goto shutdown;   
        }

        // got an address
        // send series of resets

        // we send 3 times to make sure it makes it
        send_reset( state );
        TMR_WAIT( pt, 50 );
        send_reset( state );
        TMR_WAIT( pt, 50 );
        send_reset( state );

        // listen for message.
        THREAD_WAIT_WHILE( pt, sock_i16_get_bytes_read( state->sock ) < 0 );
        
        if( sock_i16_get_bytes_read( state->sock ) == 0 ){

            continue;
        }

        // get message
        msgflow_header_t *header = sock_vp_get_data( state->sock );

        if( !validate_header( header ) ){

            continue;
        }

        // we are specifically looking for the ready message
        if( header->type != MSGFLOW_TYPE_READY ){

            // if we didn't get ready, we will just loop and reset until we hear the sink message again.

            continue;
        }

        msgflow_msg_ready_t *ready = (msgflow_msg_ready_t *)( header + 1 );

        // confirm ready matches sequence
        if( ready->sequence != state->sequence ){

            continue;
        }

        // ***********************************************************8
        // we are now connected!
        log_v_debug_P( PSTR("msgflow ready") );        



    }


shutdown:
    
    // send stop message

    // we send 3 times to make sure it makes it
    send_stop( state );
    TMR_WAIT( pt, 50 );
    send_stop( state );
    TMR_WAIT( pt, 50 );
    send_stop( state );
    TMR_WAIT( pt, 50 );

    sock_v_release( state->sock );

    log_v_debug_P( PSTR("msgflow ended") );
    
PT_END( pt );
}


#endif