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

#ifdef ENABLE_TIME_SYNC

#include "config.h"
#include "timesync.h"
#include "hash.h"
#include "sntp.h"



static int16_t clock_offset;



#include "pixel.h"

#define STROBE PIX_CLK_PORT.OUTSET = ( 1 << PIX_CLK_PIN ); \
            _delay_us(10); \
            PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN )

#define TICKS_PER_SECOND    31250
static uint16_t base_rate = TICKS_PER_SECOND;
static volatile uint16_t timer_rate = TICKS_PER_SECOND;

ISR(GFX_TIMER_CCC_vect){

    STROBE;

    GFX_TIMER.CCC += timer_rate;
}



PT_THREAD( time_server_thread( pt_t *pt, void *state ) );
PT_THREAD( time_master_thread( pt_t *pt, void *state ) );
PT_THREAD( time_clock_thread( pt_t *pt, void *state ) );

static socket_t sock;

static uint32_t local_time;
static uint32_t net_time;
static ip_addr_t master_ip;
static uint64_t master_uptime;



static uint8_t sync_state;
#define STATE_WAIT              0
#define STATE_MASTER            1
#define STATE_SLAVE             2


static uint32_t rtt_start;


KV_SECTION_META kv_meta_t time_info_kv[] = {
    { SAPPHIRE_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, &net_time,         0,                      "net_time" },
    { SAPPHIRE_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,        0,                      "net_time_master_ip" },
};


void time_v_init( void ){

    PIXEL_EN_PORT.OUTSET = ( 1 << PIXEL_EN_PIN );
    PIX_CLK_PORT.DIRSET = ( 1 << PIX_CLK_PIN );
    PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN );

    GFX_TIMER.INTCTRLB |= TC_CCCINTLVL_HI_gc;

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }


    sync_state = STATE_WAIT;

    sock = sock_s_create( SOCK_DGRAM );

    sock_v_bind( sock, TIME_SERVER_PORT );


    thread_t_create( time_server_thread,
                    PSTR("time_server"),
                    0,
                    0 );    

    thread_t_create( time_master_thread,
                    PSTR("time_master"),
                    0,
                    0 );    

    thread_t_create( time_clock_thread,
                    PSTR("time_clock"),
                    0,
                    0 );    
}

uint32_t time_u32_get_network_time( void ){

    int32_t elapsed = tmr_u32_elapsed_time_ms( local_time );

    return net_time + elapsed;
}



PT_THREAD( time_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, 1000 );


        int16_t adjustment = clock_offset;

        if( adjustment > 5 ){

            adjustment = 5;
        }
        else if( adjustment < -5 ){

            adjustment = -5;
        }

        net_time -= adjustment;

        clock_offset -= adjustment;

        ATOMIC;

        uint16_t timer_cnt = GFX_TIMER.CNT;
        uint16_t timer_cc = GFX_TIMER.CCC;
        uint32_t current_net_time = time_u32_get_network_time();

        uint16_t current_net_time_offset = current_net_time % 1000;
        uint16_t current_net_time_ticks = ( (uint32_t)current_net_time_offset * base_rate ) / 1000;

        uint16_t actual_next_cc;
        if( timer_cc > timer_cnt ){

            actual_next_cc = timer_cc - timer_cnt;
        }
        else{

            actual_next_cc = timer_cc + ( 65535 - timer_cnt );
        }

        uint16_t correct_next_cc = base_rate - current_net_time_ticks;

        int16_t timer_error = (int32_t)actual_next_cc - (int32_t)correct_next_cc;
        
        if( abs16( timer_error ) > 2000 ){

            // course adjustment
            GFX_TIMER.CCC -= timer_error;
        }
        else{
            // fine adjustment
            if( timer_error > 10 ){

                timer_rate = base_rate - 100;
            }
            else if( timer_error < -10 ){

                timer_rate = base_rate + 100;
            }
        }

        END_ATOMIC;

        // log_v_debug_P( PSTR("timer err: %d net offset: %u ticks: %u cc actual: %u correct: %u"),
        //     timer_error, current_net_time_offset, current_net_time_ticks, actual_next_cc, correct_next_cc );
        log_v_debug_P( PSTR("timer err: %d"), timer_error );

    }

PT_END( pt );
}


PT_THREAD( time_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // set timeout (in seconds)
    sock_v_set_timeout( sock, 32 );
    
    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

            uint32_t *magic = sock_vp_get_data( sock );

            if( *magic != TIME_PROTOCOL_MAGIC ){

                continue;
            }

            uint8_t *version = (uint8_t *)(magic + 1);

            if( *version != TIME_PROTOCOL_VERSION ){

                continue;
            }

            uint8_t *type = version + 1;

            sock_addr_t raddr;
            sock_v_get_raddr( sock, &raddr );

            if( *type == TIME_MSG_MASTER ){

                time_msg_master_t *msg = (time_msg_master_t *)magic;

                if( sync_state == STATE_WAIT ){

                    // select master
                    master_ip = raddr.ipaddr;
                    master_uptime = msg->uptime;
                    
                    sync_state = STATE_SLAVE;

                    log_v_debug_P( PSTR("assigning master: %d.%d.%d.%d"), 
                        master_ip.ip3, 
                        master_ip.ip2, 
                        master_ip.ip1, 
                        master_ip.ip0 );                    
                }
                else if( sync_state == STATE_MASTER ){

                    log_v_debug_P( PSTR("rx sync while master") );

                    // check if this master is better
                    if( msg->uptime > master_uptime ){

                        // select master
                        master_ip = raddr.ipaddr;
                        master_uptime = msg->uptime;
                    
                        sync_state = STATE_SLAVE;

                        log_v_debug_P( PSTR("assigning new master: %d.%d.%d.%d"), 
                            master_ip.ip3, 
                            master_ip.ip2, 
                            master_ip.ip1, 
                            master_ip.ip0 );                    
                    }
                } 
            }
            else if( *type == TIME_MSG_NOT_MASTER ){

                // check if that was master.
                if( ip_b_addr_compare( raddr.ipaddr, master_ip ) ){

                    // ruh roh.  master rebooted or something.
                    sync_state = STATE_WAIT;

                    log_v_debug_P( PSTR("lost master, resetting state") );
                }
            }
            else if( *type == TIME_MSG_REQUEST_SYNC ){

                if( sync_state != STATE_MASTER ){

                    continue;
                }

                net_time = tmr_u32_get_system_time_ms();
                local_time = net_time;

                time_msg_sync_t msg;
                msg.magic           = TIME_PROTOCOL_MAGIC;
                msg.version         = TIME_PROTOCOL_VERSION;
                msg.type            = TIME_MSG_SYNC;
                msg.net_time        = net_time;
                msg.uptime          = master_uptime;
                msg.flags           = 0;

                sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
            }
            else if( *type == TIME_MSG_SYNC ){

                time_msg_sync_t *msg = (time_msg_sync_t *)magic;

                uint32_t now = tmr_u32_get_system_time_ms();
                uint32_t elapsed = tmr_u32_elapsed_times( rtt_start, now );

                if( elapsed > 500 ){

                    // a 0.5 second RTT is clearly ridiculous.
                    log_v_debug_P( PSTR("bad: RTT: %u"), elapsed );

                    continue;
                }

                uint32_t est_net_time = time_u32_get_network_time();

                uint32_t adjusted_net_time = msg->net_time + ( elapsed / 2 );

                int32_t net_diff = tmr_u32_elapsed_times( adjusted_net_time, est_net_time );

                // how bad is our offset?
                if( abs32( net_diff ) > 500 ){
                    // worse than 0.5 seconds, do a hard jump

                    local_time = now;

                    net_time = adjusted_net_time;
                    net_diff = 0;

                    log_v_debug_P( PSTR("hard jump") );
                }


                ATOMIC;
                clock_offset = net_diff;
                END_ATOMIC;
                
                log_v_debug_P( PSTR("rtt: %lu offset %d"), elapsed, clock_offset );
                

                // // sync timer
                // ATOMIC;

                // est_net_time = time_u32_get_network_time();
                // uint32_t timer_target = ( ( 1000 - ( est_net_time % 1000 ) ) * 31250 ) / 1000;

                // if( timer_target < 10000 ){

                //     timer_target += 31250;
                // }

                // GFX_TIMER.CCC = GFX_TIMER.CNT + timer_target;

                // END_ATOMIC;
            }
        }
        // socket timeout
        else{

            if( sync_state != STATE_MASTER ){

                log_v_debug_P( PSTR("timed out, resetting state") );

                sync_state = STATE_WAIT;
            }
        }
    }


PT_END( pt );
}

static void send_master( void ){

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    net_time = tmr_u32_get_system_time_ms();
    local_time = net_time;

    time_msg_master_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_MASTER;
    msg.net_time        = net_time;
    msg.uptime          = master_uptime;
    msg.flags           = 0;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );   
}

static void send_not_master( void ){

    time_msg_not_master_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_NOT_MASTER;
    
    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}

static void request_sync( void ){

    time_msg_request_sync_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_REQUEST_SYNC;
    
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = master_ip;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}


PT_THREAD( time_master_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
        
    master_ip = ip_a_addr(0,0,0,0);

    THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

    // random delay, see if other masters show up
    if( sync_state == STATE_WAIT ){

        // send, broadcast a message staying we are not a master.
        // this way, if we *were* the master and got reset, then when we come back up
        // we'll tell everyone else we aren't the master, so someone else will jump in
        // (with the current network time).

        send_not_master();
        TMR_WAIT( pt, 200 );
        send_not_master();

        TMR_WAIT( pt, 2000 + ( rnd_u16_get_int() >> 3 ) );
    }

    if( sync_state == STATE_WAIT ){

        // elect ourselves as master
        sync_state = STATE_MASTER;
        master_uptime = 0;

        log_v_debug_P( PSTR("we are master") );
    }

    if( sync_state == STATE_MASTER ){

        sntp_v_start();
    }

    while( sync_state == STATE_MASTER ){

        TMR_WAIT( pt, TIME_MASTER_SYNC_RATE * 1000 );

        // check state
        if( sync_state != STATE_MASTER ){

            // no longer master
            log_v_debug_P( PSTR("no longer master") );

            sntp_v_stop();

            break;
        }

        master_uptime += TIME_MASTER_SYNC_RATE;

        send_master();
    }

    while( sync_state == STATE_SLAVE ){

        // random delay
        TMR_WAIT( pt, ( TIME_SLAVE_SYNC_RATE_BASE * 1000 ) + ( rnd_u16_get_int() >> 3 ) );

        // check state
        if( sync_state != STATE_SLAVE ){

            // no longer master
            log_v_debug_P( PSTR("no longer slave") );

            break;
        }
        
        rtt_start = tmr_u32_get_system_time_ms();

        request_sync();
    }

    // restart if we get here
    THREAD_RESTART( pt );

PT_END( pt );
}


#endif
