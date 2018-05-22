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




#include "pixel.h"

#define STROBE PIX_CLK_PORT.OUTSET = ( 1 << PIX_CLK_PIN ); \
            _delay_us(10); \
            PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN )

#define TICKS_PER_SECOND    31250

#define BASE_RATE_MS        250

static uint16_t base_rate = ( (uint32_t)BASE_RATE_MS * TICKS_PER_SECOND ) / 1000;
static volatile uint16_t timer_rate;

static volatile uint16_t frame_number;
static uint32_t frame_base_time;

ISR(GFX_TIMER_CCC_vect){

    frame_number++;

    GFX_TIMER.CCC += timer_rate;


    if( ( frame_number % 4 ) == 0 ){

        STROBE;
    }
}



PT_THREAD( time_server_thread( pt_t *pt, void *state ) );
PT_THREAD( time_master_thread( pt_t *pt, void *state ) );
PT_THREAD( time_clock_thread( pt_t *pt, void *state ) );

static socket_t sock;

static uint32_t local_time;
static uint32_t last_net_time;
static uint32_t net_time;
static ip_addr_t master_ip;
static uint64_t master_uptime;

// static int16_t filtered_offset;
// #define OFFSET_FILTER           32

static uint8_t sync_state;
#define STATE_WAIT              0
#define STATE_MASTER            1
#define STATE_SLAVE             2


#define DRIFT_FILTER            32
static uint8_t drift_init;
static int16_t filtered_drift;

static uint32_t rtt_start;
static uint16_t filtered_rtt;
#define RTT_FILTER              32
#define RTT_QUALITY_LIMIT       10
#define RTT_RELAX               4


KV_SECTION_META kv_meta_t time_info_kv[] = {
    { SAPPHIRE_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, &net_time,         0,                      "net_time" },
    { SAPPHIRE_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,        0,                      "net_time_master_ip" },
};


void time_v_init( void ){

    timer_rate = base_rate;

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

    // now adjust for drift
    int32_t drift = ( filtered_drift * elapsed ) / 65536;

    uint32_t adjusted_net_time = net_time + ( elapsed - drift );

    return adjusted_net_time;
}



PT_THREAD( time_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, 1000 );

        // first, adjust the network timer.  this just does the fine adjustment.
        // the coarse adjustment is done by the time server when the sync message comes in. 

        // int16_t adjustment = clock_adjustment;

        // if( adjustment > 40 ){

        //     adjustment = 20;
        // }
        // else if( adjustment > 20 ){

        //     adjustment = 10;
        // }
        // else if( adjustment > 5 ){

        //     adjustment = 5;
        // }
        // else if( adjustment < -40 ){

        //     adjustment = -20;
        // }
        // else if( adjustment < -20 ){

        //     adjustment = -10;
        // }
        // else if( adjustment < -5 ){

        //     adjustment = -5;
        // }

        // log_v_debug_P( PSTR("clock_adjustment: %d -> %d"), clock_adjustment, adjustment );

        // net_time -= adjustment;

        // clock_adjustment -= adjustment;


        // now adjust the real time clock

        ATOMIC;

        /*
    
        Get current network time offset and clock offset.
        Calculate the timer error.
        
        */

        uint16_t timer_cnt = GFX_TIMER.CNT;
        uint32_t current_net_time = time_u32_get_network_time();

        uint16_t timer_cc = GFX_TIMER.CCC;
        uint32_t base_frame_time = (uint32_t)frame_number * base_rate;

        // uint16_t actual_next_cc;
        // if( timer_cc > timer_cnt ){

        //     actual_next_cc = timer_cc - timer_cnt;
        // }
        // else{

        //     actual_next_cc = timer_cc + ( 65535 - timer_cnt );
        // }

        uint16_t last_cc = timer_cc - base_rate;
        uint16_t elapsed_ticks;

        if( timer_cnt > last_cc ){

            elapsed_ticks = timer_cnt - last_cc;
        }
        else{

            elapsed_ticks = timer_cnt + ( 65535 - last_cc );
        }


        

        // uint32_t frame_time = base_frame_time + ( (int32_t)actual_next_cc - base_rate );
        uint32_t frame_time = base_frame_time + elapsed_ticks;




        // uint16_t current_net_time_offset = current_net_time % ( ( (uint32_t)base_rate * 1000 ) / TICKS_PER_SECOND );
        // uint16_t current_net_time_ticks = ( (uint32_t)current_net_time_offset * TICKS_PER_SECOND ) / 1000;

        uint32_t current_net_time_ticks = ( (uint64_t)current_net_time * TICKS_PER_SECOND ) / 1000;

        int16_t frame_offset_ticks = (int64_t)current_net_time_ticks - (int64_t)frame_time;

        // log_v_debug_P( PSTR("%u %u %u %u %lu %lu %d"), timer_cnt, timer_cc, last_cc, elapsed_ticks, frame_time, current_net_time_ticks, frame_offset_ticks );
        log_v_debug_P( PSTR("%d"), frame_offset_ticks );
        

        static uint32_t last_net_time_ticks;
        static uint32_t last_frame_time_ticks;


        

        uint16_t net_frame = current_net_time_ticks / base_rate;
        uint32_t net_frame_time = (uint32_t)net_frame * base_rate;
        uint16_t net_frame_offset = current_net_time_ticks - net_frame_time;

        
        // uint32_t frame_time_ticks = frame_time + actual_next_cc;


        // int16_t offset = ( (int32_t)net_frame_offset + (int32_t)actual_next_cc ) % base_rate;

        // int16_t frame_diff = (int32_t)net_frame - (int32_t)frame_number;

        // log_v_debug_P( PSTR("%u %u %lu %lu %lu %u"), timer_cnt, timer_cc, current_net_time_ticks, frame_time, current_net_time_ticks - frame_time, actual_next_cc );

        // int16_t offset = (int32_t)actual_next_cc - (int32_t)net_frame_offset;

        // log_v_debug_P( PSTR("%lu %u %lu %u %d"), frame_time, actual_next_cc, net_frame_time, net_frame_offset, offset );
        // log_v_debug_P( PSTR("%lu %lu %u %u %lu"), frame_time_ticks - last_frame_time_ticks, current_net_time_ticks - last_net_time_ticks, frame_number, net_frame, current_net_time_ticks );

        // log_v_debug_P( PSTR("%u %u %lu %lu %lu %lu %lu %u"), frame_number, net_frame, frame_time, current_net_time_ticks, current_net_time_ticks - frame_time, frame_time - last_frame_time_ticks, current_net_time_ticks- last_net_time_ticks, elapsed_ticks );

        last_frame_time_ticks = frame_time;
        last_net_time_ticks = current_net_time_ticks;

        // log_v_debug_P( PSTR("net: %lu ticks: %lu frame_time: %lu frame: %u netframe: %u offset: %u actual_cc: %u ++ %u"), 
                // current_net_time, current_net_time_ticks, frame_time, frame_number, net_frame, net_frame_offset, actual_next_cc, net_frame_offset + actual_next_cc );

        // log_v_debug_P( PSTR("frame %d offset: %d"), frame_diff, offset );
        
        if( abs32( (int32_t)frame_number - (int32_t)net_frame ) > 3 ){

            log_v_debug_P( PSTR("hard frame sync") );
            frame_number = net_frame;
            GFX_TIMER.CCC = GFX_TIMER.CNT + base_rate;   
        }

        // uint16_t correct_next_cc = base_rate - current_net_time_ticks;

        // int16_t timer_error = (int32_t)actual_next_cc - (int32_t)correct_next_cc;

        int16_t timer_error = frame_offset_ticks;

        /*
    
        Clock adjustment.
    
        If the clock is "way off", adjust in one hard step.

        Then, do fine adjustments to the clock speed to bring it into alignment.

        */

        int16_t timer_adjust = 0;

        // fine adjustment
        if( timer_error > 1000 ){

            timer_adjust = -200;
        }
        else if( timer_error > 200 ){

            timer_adjust = -30;
        }
        else if( timer_error > 100 ){

            timer_adjust = -10;
        }
        else if( timer_error > 10 ){

            timer_adjust = -5;
        }

        else if( timer_error < -1000 ){

            timer_adjust = 200;
        }
        else if( timer_error < -200 ){

            timer_adjust = 30;
        }
        else if( timer_error < -100 ){

            timer_adjust = 10;
        }
        else if( timer_error < -10 ){

            timer_adjust = 5;
        }
        else{

            timer_adjust = 0;                
        }

        timer_rate = base_rate + timer_adjust;
    

        

        END_ATOMIC;

        // log_v_debug_P( PSTR("tmr err: %d adj: %d"), timer_error, timer_adjust );

        // log_v_debug_P( PSTR("tmr err: %d adj: %d -> %d"), timer_error, adjustment, clock_adjustment );
        // log_v_debug_P( PSTR("net: %lu offset: %u ticks: %u cc correct: %u actual: %u frame time: %lu"), 
            // current_net_time, current_net_time_offset, current_net_time_ticks, correct_next_cc, actual_next_cc, frame_time );

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
                    filtered_drift = 0;
                    drift_init = 0;
                    filtered_rtt = 0;

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
                        filtered_drift = 0;
                        drift_init = 0;
                        filtered_rtt = 0;
                    
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

                // STROBE;
            }
            else if( *type == TIME_MSG_SYNC ){

                // STROBE;

                time_msg_sync_t *msg = (time_msg_sync_t *)magic;

                uint32_t now = tmr_u32_get_system_time_ms();
                uint32_t est_net_time = time_u32_get_network_time();

                int32_t elapsed_rtt = tmr_u32_elapsed_times( rtt_start, now );
                if( elapsed_rtt > 500 ){

                    // a 0.5 second RTT is clearly ridiculous.
                    log_v_debug_P( PSTR("bad: RTT: %u"), elapsed_rtt );

                    continue;
                }
                // check quality of RTT sample
                else if( ( filtered_rtt > 0 ) && 
                         ( elapsed_rtt > ( filtered_rtt + RTT_QUALITY_LIMIT ) ) ){

                    log_v_debug_P( PSTR("poor quality: RTT: %u"), elapsed_rtt );


                    // although we are not using this sync message for our clock,
                    // we will bump the filtered RTT up a bit in case the overall RTT is
                    // drifting upwards.  this prevents us from losing sync on a busy network.
                    // also - only do this if RTT has been initiatized.
                    if( filtered_rtt > 0 ){
                        
                        filtered_rtt += RTT_RELAX;
                    }

                    continue;
                }

                int32_t elapsed_local = tmr_u32_elapsed_times( local_time, now );  
                local_time = now;

                // adjust network timestamp from server with RTT measurement
                uint32_t adjusted_net_time = msg->net_time + ( elapsed_rtt / 2 );

                int32_t elapsed_remote_net = tmr_u32_elapsed_times( last_net_time, adjusted_net_time );
                last_net_time = adjusted_net_time;

                // compute drift
                int32_t clock_diff = elapsed_local - elapsed_remote_net;
                int16_t drift = ( clock_diff * 65536 ) / elapsed_remote_net;

                if( drift_init < 1 ){

                    drift_init++;
                }
                else if( drift_init == 1 ){

                    drift_init++;
                    filtered_drift = drift;
                    filtered_rtt = elapsed_rtt;
                }
                else{

                    filtered_drift = util_i16_ewma( drift, filtered_drift, DRIFT_FILTER );
                    filtered_rtt = util_u16_ewma( elapsed_rtt, filtered_rtt, RTT_FILTER );

                    if( drift_init < 255 ){

                        drift_init++;
                    }
                }

                // compute offset between actual network time and what our internal estimate was
                int16_t clock_offset = (int64_t)est_net_time - (int64_t)adjusted_net_time;


                // how bad is our offset?
                if( abs16( clock_offset ) > 500 ){
                    // worse than 0.5 seconds, do a hard jump

                    net_time = adjusted_net_time;

                    // reset drift filter
                    drift = 0;
                    drift_init = 0;
                    filtered_drift = 0;

                    filtered_rtt = 0;
                
                    log_v_debug_P( PSTR("hard jump: %ld"), clock_offset );

                    // filtered_offset = 0;


                    // frame sync:
                    ATOMIC;
                    frame_base_time = net_time;
                    frame_number = 0;
                    END_ATOMIC;
                }
                else{

                    net_time += elapsed_remote_net;

                    // if( filtered_offset == 0 ){

                    //     filtered_offset = clock_offset;
                    // }
                    // else{

                    //     filtered_offset = util_i16_ewma( clock_offset, filtered_offset, OFFSET_FILTER );
                    // }
                }

                // log_v_debug_P( PSTR("rtt: %lu filt: %u offset %d drift: %d"), 
                    // elapsed_rtt, filtered_rtt, clock_offset, filtered_drift );
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

        // TMR_WAIT( pt, 2000 + ( rnd_u16_get_int() >> 3 ) );
        TMR_WAIT( pt, rnd_u16_get_int() >> 5 );
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

        // check state
        if( sync_state != STATE_MASTER ){

            // no longer master
            log_v_debug_P( PSTR("no longer master") );

            sntp_v_stop();

            break;
        }

        master_uptime += TIME_MASTER_SYNC_RATE;

        send_master();

        TMR_WAIT( pt, TIME_MASTER_SYNC_RATE * 1000 );
    }

    while( sync_state == STATE_SLAVE ){

        // random delay
        uint16_t delay;

        if( drift_init > 16 ){

            delay = ( TIME_SLAVE_SYNC_RATE_MAX * 1000 ) + ( rnd_u16_get_int() >> 3 );
        }
        else{

            delay = ( TIME_SLAVE_SYNC_RATE_BASE * 1000 ) + ( rnd_u16_get_int() >> 3 );
        }

        TMR_WAIT( pt, delay );

        // check state
        if( sync_state != STATE_SLAVE ){

            // no longer master
            log_v_debug_P( PSTR("no longer slave") );

            break;
        }
        
        rtt_start = tmr_u32_get_system_time_ms();

        request_sync();
        // STROBE;
    }

    // restart if we get here
    THREAD_RESTART( pt );

PT_END( pt );
}


#endif
