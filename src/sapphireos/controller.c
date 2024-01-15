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
#include "config.h"

#include "controller.h"


/*

Controller selection algorithm:


Election States:

Idle: Node has initialized and is waiting for something to do.
Follower: node is a follower
	This is the default state, even if there is no leader
Candidate: node is a candidate to become leader	
Leader: node is the leader


State transitions:

Idle:
	-> Follower: on timeout, leader is available
	-> Candidate: on timeout, leader is not available, and leader mode enabled

Follower:
	-> Idle: on timeout (no announcements received)
	-> Idle: drop message received from leader

Candidate:
	-> Follower: on timeout, better leader is available
	-> Leader: on timeout, no better leader found

Leader:
	-> Idle: Better leader found
	-> Idle: Follower count reaches zero



Announce messages every 2 seconds

Init:
Start in follower state.
Wait 10 seconds, if no leader, wait some random delay, then start
announce and switch to candidate state (if controller leader mode)


Leader loss:
Go back to init




Candidates broadcast their announcements with their
priorities and follower count.

Candidates and passives will select the highest priority
and highest follower count (priority first, then count)
and register with that node, increasing its follower count.

Candidates that switch to follower will send a message to drop
their followers.

Followers will also send a leave message to their candidates
when they switch.

After a timeout, candidates with non-zero follower counts will
switch to leader.

If a node is a leader and hears a better leader, it will
reset its state machine and drop its followers


Messages:

Announce: Broadcast candidacy or leadership.  
Contains priority and follower count.

Drop: Inform a follower that this node is not a leader.

Status: Update follower status for a candidate or leader.
Status is unicast and will perform a join operation on that
leader/candidate.  Status is sent periodically and will reset
the follower timeout.  Status is only send to the current leader
for that follower.

Leave: A follower informs a leader that it is leaving.
This purges the follower from the leader.  It is also
optional, merely withholding status will trigger a purge
by the timeout, but the leave operation accomplishes the 
task more quickly.






*/


#ifdef ENABLE_CONTROLLER

static socket_t sock;

static bool controller_enabled;

static uint8_t controller_state;
#define STATE_IDLE 		0
#define STATE_FOLLOWER 	1
#define STATE_CANDIDATE 2
#define STATE_LEADER 	3


static catbus_string_t state_name;

static ip_addr4_t leader_ip;
static uint16_t leader_priority;
static uint16_t leader_follower_count;
static uint16_t leader_timeout;

KV_SECTION_META kv_meta_t controller_kv[] = {
    { CATBUS_TYPE_UINT8, 	0, KV_FLAGS_READ_ONLY, &controller_state, 		0,  "controller_state" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, &state_name,				0,  "controller_state_text" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_PERSIST,   &controller_enabled, 	0,  "controller_enable_leader" },
    { CATBUS_TYPE_IPv4, 	0, KV_FLAGS_READ_ONLY, &leader_ip, 				0,  "controller_leader_ip" },
    { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, &leader_follower_count, 	0,  "controller_follower_count" },
    { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, &leader_priority, 		0,  "controller_leader_priority" },
};

typedef struct{
	ip_addr4_t ip;
	uint16_t timeout;
} follower_t;

static list_t follower_list;


PT_THREAD( controller_announce_thread( pt_t *pt, void *state ) );
PT_THREAD( controller_server_thread( pt_t *pt, void *state ) );
PT_THREAD( controller_timeout_thread( pt_t *pt, void *state ) );


static PGM_P get_state_name( uint8_t state ){

	if( state == STATE_IDLE ){

		return PSTR("idle");
	}
	else if( state == STATE_FOLLOWER ){

		return PSTR("follower");
	}
	else if( state == STATE_CANDIDATE ){

		return PSTR("candidate");
	}
	else if( state == STATE_LEADER ){

		return PSTR("leader");
	}
	else{

		return PSTR("unknown");
	}
}

static void apply_state_name( void ){

	strncpy_P( state_name.str, get_state_name( controller_state ), sizeof(state_name.str) );
}

static void set_state( uint8_t state ){

	controller_state = state;
	apply_state_name();
}

static void update_follower_timeouts( void ){

	list_node_t ln = follower_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        follower_t *follower = (follower_t *)list_vp_get_data( ln );

        if( follower->timeout > 0 ){

        	follower->timeout--;

        	if( follower->timeout == 0 ){

	    		log_v_debug_P( PSTR("Follower timed out: %d.%d.%d.%d"),
					follower->ip.ip3, 
					follower->ip.ip2, 
					follower->ip.ip1, 
					follower->ip.ip0
				 );

        		list_v_remove( &follower_list, ln );
	            list_v_release_node( ln );
        	}
        }

        ln = next_ln;
    }	
}

static void remove_follower( ip_addr4_t follower_ip ){

	list_node_t ln = follower_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        follower_t *follower = (follower_t *)list_vp_get_data( ln );

        if( ip_b_addr_compare( follower_ip, follower->ip ) ){

        	log_v_debug_P( PSTR("Removing follower: %d.%d.%d.%d"),
				follower_ip.ip3, 
				follower_ip.ip2, 
				follower_ip.ip1, 
				follower_ip.ip0
			);

    		list_v_remove( &follower_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }	
}

static void update_follower( ip_addr4_t follower_ip ){

	// search for follower:
	// if found, update
	// if not found, add
	list_node_t ln = follower_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        follower_t *follower = (follower_t *)list_vp_get_data( ln );

        if( ip_b_addr_compare( follower_ip, follower->ip ) ){

        	follower->timeout = CONTROLLER_FOLLOWER_TIMEOUT;

        	return;
        }

        ln = next_ln;
    }		

    follower_t follower = {
    	follower_ip,
		CONTROLLER_FOLLOWER_TIMEOUT,
    };

    // not found, add:
    ln = list_ln_create_node( &follower, sizeof(follower) );

    if( ln < 0 ){

        return;
    }

 	list_v_insert_tail( &follower_list, ln );   
}

static void reset_leader( void ){

	leader_ip = ip_a_addr(0,0,0,0);
	leader_priority = 0;
	leader_follower_count = 0;
}

static uint16_t get_follower_count( void ){

	return list_u8_count( &follower_list );
}

static uint16_t get_priority( void ){

	// check if leader is enabled, if not, return 0 and we are follower only
	if( !controller_enabled ){

		return 0;
	}

	uint16_t priority = 0;

	// set base priorities

	#ifdef ESP32
	priority = 256;
	#else
	priority = 128;
	#endif

	return priority;
}

static void vote( ip_addr4_t ip, uint16_t priority, uint16_t follower_count ){

	leader_ip = ip;
	leader_priority = priority;
	leader_follower_count = follower_count;
	leader_timeout = CONTROLLER_FOLLOWER_TIMEOUT;
}

static void vote_self( void ){

	vote( cfg_ip_get_ipaddr(), get_priority(), get_follower_count() );
}

void controller_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
    }

    list_v_init( &follower_list );	

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, CONTROLLER_PORT );

    thread_t_create( controller_server_thread,
                     PSTR("controller_server"),
                     0,
                     0 );


    thread_t_create( controller_announce_thread,
                     PSTR("controller_announce"),
                     0,
                     0 );

    thread_t_create( controller_timeout_thread,
                     PSTR("controller_timeout"),
                     0,
                     0 );
}

// static bool is_candidate( void ){

// 	return controller_enabled && ip_b_is_zeroes( leader_ip );
// }

static void send_msg( uint8_t msgtype, uint8_t *msg, uint8_t len, sock_addr_t *raddr ){

	controller_header_t *header = (controller_header_t *)msg;

	header->magic 		= CONTROLLER_MSG_MAGIC;
	header->version 	= CONTROLLER_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->reserved    = 0;

	sock_i16_sendto( sock, msg, len, raddr );
}

static void send_announce( void ){

	if( controller_state == STATE_FOLLOWER ){

		log_v_error_P( PSTR("cannot send announce as follower!") );

		return;
	}

    uint16_t flags = 0;

    if( controller_state == STATE_LEADER ){

    	flags |= CONTROLLER_FLAGS_IS_LEADER;
    }

	controller_msg_announce_t msg = {
		{ 0 },
		flags,
		get_priority(),
		get_follower_count(),
		// tmr_u64_get_system_time_us(),
		// cfg_u64_get_device_id(),
	};

	sock_addr_t raddr;
    raddr.ipaddr = ip_a_addr(255, 255, 255, 255);
    raddr.port = CONTROLLER_PORT;

	send_msg( CONTROLLER_MSG_ANNOUNCE, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void send_status( void ){

	if( controller_state == STATE_LEADER ){

		log_v_error_P( PSTR("cannot send status as leader!") );

		return;
	}

	if( ip_b_is_zeroes( leader_ip ) ){

		log_v_error_P( PSTR("cannot send status, no leader!") );

		return;
	}

	controller_msg_status_t msg = {
		{ 0 },
	};

	sock_addr_t raddr;
    raddr.ipaddr = ip_a_addr(255, 255, 255, 255);
    raddr.port = CONTROLLER_PORT;

	send_msg( CONTROLLER_MSG_STATUS, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void send_leave( void ){

	if( controller_state == STATE_LEADER ){

		log_v_error_P( PSTR("cannot send leave as leader!") );

		return;
	}

	if( ip_b_is_zeroes( leader_ip ) ){

		log_v_error_P( PSTR("cannot send leave, no leader!") );

		return;
	}

	controller_msg_leave_t msg = {
		{ 0 },
	};

	sock_addr_t raddr;
    raddr.ipaddr = leader_ip;
    raddr.port = CONTROLLER_PORT;

	send_msg( CONTROLLER_MSG_LEAVE, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void send_drop( ip_addr4_t ip ){

	controller_msg_drop_t msg = {
		{ 0 },
	};

	sock_addr_t raddr;
    raddr.ipaddr = ip;
    raddr.port = CONTROLLER_PORT;

	send_msg( CONTROLLER_MSG_DROP, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void process_announce( controller_msg_announce_t *msg, sock_addr_t *raddr ){

	bool update_leader = FALSE;

	// check if this is the first leader/candidate we've seen
	if( !ip_b_is_zeroes( leader_ip ) ){

		update_leader = TRUE;
	}
	// check if already tracking this leader:
	else if( !ip_b_is_zeroes( leader_ip ) && 
		     ip_b_addr_compare( raddr->ipaddr, leader_ip ) ){

		update_leader = TRUE;
	}
	// check better priority:
	else if( msg->priority > leader_priority ){

		update_leader = TRUE;
	}
	// check same priority
	else if( msg->priority == leader_priority ){

		// is follower count better?
		if( msg->follower_count > leader_follower_count ){

			update_leader = TRUE;	
		}
		// is follower count the same?
		else if( msg->follower_count == leader_follower_count ){

			// choose lowest IP to resolve an 
			// even follower count.
			uint32_t msg_ip_u32 = ip_u32_to_int( raddr->ipaddr );
			uint32_t leader_ip_u32 = ip_u32_to_int( leader_ip );

			if( msg_ip_u32 < leader_ip_u32 ){

				update_leader = TRUE;	
			}
		}
	}

	if( update_leader ){

		// check if switching leaders
		if( !ip_b_addr_compare( leader_ip, raddr->ipaddr ) ){

			log_v_debug_P( PSTR("Switching leader to: %d.%d.%d.%d"),
				raddr->ipaddr.ip3, 
				raddr->ipaddr.ip2, 
				raddr->ipaddr.ip1, 
				raddr->ipaddr.ip0
			);

			// if we were tracking a pre-existing leader,
			// inform it that we are leaving.
			if( !ip_b_is_zeroes( leader_ip ) ){

				log_v_debug_P( PSTR("Leaving previous leader") );

				// byeeeeeeeeee
				send_leave();	
			}
			// check if we were *a* leader, but this one is better
			else if( ip_b_addr_compare( cfg_ip_get_ipaddr(), leader_ip ) ){

				log_v_debug_P( PSTR("Dropping all followers") );

				// drop followers
				send_drop( ip_a_addr(255,255,255,255) );
				send_drop( ip_a_addr(255,255,255,255) );
				send_drop( ip_a_addr(255,255,255,255) );

				list_v_destroy( &follower_list );
			}
		}

		vote( raddr->ipaddr, msg->priority, msg->follower_count );
	}
}


static void process_status( controller_msg_status_t *msg, sock_addr_t *raddr ){

	if( controller_state == STATE_FOLLOWER ){

		// we are a follower, we don't care about status.

		// this is mostly erroneous, the sender will
		// figure out we aren't a leader eventually.
		// we won't send a drop message in this case
		// since we aren't changing our own state.

		return;
	}

	// add or update this node's tracking information
	update_follower( raddr->ipaddr );
}

static void process_leave( controller_msg_leave_t *msg, sock_addr_t *raddr ){

	if( controller_state == STATE_FOLLOWER ){

		// we are a follower, we don't care about leave.

		return;
	}

	// remove this node from tracking
	remove_follower( raddr->ipaddr );
}

static void process_drop( controller_msg_drop_t *msg, sock_addr_t *raddr ){

	// check if this is from our leader
	if( ip_b_addr_compare( leader_ip, raddr->ipaddr ) ){

		log_v_debug_P( PSTR("Dropping leader: %d.%d.%d.%d"),
			raddr->ipaddr.ip3, 
			raddr->ipaddr.ip2, 
			raddr->ipaddr.ip1, 
			raddr->ipaddr.ip0
		);

		reset_leader();
	}
}

PT_THREAD( controller_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
   	
   	while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        uint16_t error = CATBUS_STATUS_OK;

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        controller_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->magic != CONTROLLER_MSG_MAGIC ){

            goto end;
        }

        if( header->version != CONTROLLER_MSG_VERSION ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->msg_type == CONTROLLER_MSG_ANNOUNCE ){

        	process_announce( (controller_msg_announce_t *)header, &raddr );
        }
        else if( header->msg_type == CONTROLLER_MSG_DROP ){

        	process_drop( (controller_msg_drop_t *)header, &raddr );
        }
        else if( header->msg_type == CONTROLLER_MSG_STATUS ){

        	process_status( (controller_msg_status_t *)header, &raddr );
        }
        else if( header->msg_type == CONTROLLER_MSG_LEAVE ){

        	process_leave( (controller_msg_leave_t *)header, &raddr );
        }
        else{

        	// invalid message
        	log_v_error_P( PSTR("Invalid msg: %d"), header->msg_type );
        }


    end:

    	THREAD_YIELD( pt );
	}
    
PT_END( pt );
}


PT_THREAD( controller_announce_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	static uint32_t start_time;

	set_state( STATE_IDLE );

	// start in idle state, to wait for announce messages

	
	// if enabled, start out as a candidate
	if( controller_enabled ){

		set_state( STATE_CANDIDATE );

		start_time = tmr_u32_get_system_time_ms();

		vote_self();
	}	
	else{

		set_state( STATE_FOLLOWER );
	}

	// random delay:
	TMR_WAIT( pt, rnd_u16_get_int() >> 4 ); // 0 - 4096 ms

	while( controller_state == STATE_CANDIDATE ){

		// broadcast announcement
		send_announce();

		// random delay:
		TMR_WAIT( pt, 1000 + ( rnd_u16_get_int() >> 5 ) ); // 1000 - 3048 ms
			
		// check if we are leader after timeout
		if( ip_b_addr_compare( leader_ip, cfg_ip_get_ipaddr() ) &&
			tmr_u32_elapsed_time_ms( start_time ) > 20000 ){

			log_v_debug_P( PSTR("Electing self as leader") );

			set_state( STATE_LEADER );
		}
	}

	while( controller_state == STATE_LEADER ){

		// broadcast announcement
		send_announce();

		// random delay:
		TMR_WAIT( pt, 1000 + ( rnd_u16_get_int() >> 5 ) ); // 1000 to 3048 ms
	}

	while( controller_state == STATE_FOLLOWER ){

		if( !ip_b_is_zeroes( leader_ip) ){

			// send status
			send_status();	
		}

		// random delay:
		TMR_WAIT( pt, 1000 + ( rnd_u16_get_int() >> 5 ) ); // 1000 to 3048 ms
	}

	TMR_WAIT( pt, 100 );

	THREAD_RESTART( pt );
    
PT_END( pt );
}


PT_THREAD( controller_timeout_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
   	
   	while(1){

   		TMR_WAIT( pt, 1000 );

   		update_follower_timeouts();

   		leader_follower_count = get_follower_count();

   		if( ( leader_timeout > 0 ) && 
   			( !ip_b_addr_compare( leader_ip, cfg_ip_get_ipaddr() ) ) ){

   			leader_timeout--;

   			if( leader_timeout == 0 ){

   				set_state( STATE_IDLE );

   				reset_leader();
   			}
   		}
	}
    
PT_END( pt );
}




#endif
