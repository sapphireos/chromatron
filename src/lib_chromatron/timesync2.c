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
#if 0

#include "sapphire.h"

#ifdef ENABLE_TIME_SYNC

#include "config.h"
#include "graphics.h"
#include "timesync.h"
#include "hash.h"
#include "vm.h"

#include <stdlib.h>

#include "pixel.h"


#define SYNC_TIMER TCE0
#define SYNC_TIMER_OVF_vect        TCE0_OVF_vect

#define SYNC_PER    31250
#define TICKS_PER_MS ( 31250 / 1000 )

#define STROBE PIX_CLK_PORT.OUTSET = ( 1 << PIX_CLK_PIN ); \
            _delay_us(10); \
            PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN )

static int16_t clock_offset;

ISR(SYNC_TIMER_OVF_vect){

    // STROBE;

    uint32_t net = time_u32_get_network_time();
    uint32_t frac = net % 1000;

    if( frac < 500 ){

        clock_offset = -1 * frac;
    }
    else{

        clock_offset = 1000 - frac;
    }

    uint16_t temp = abs(clock_offset);

    if( temp > 100 ){

        temp = 100;
    }

    if( clock_offset > 0 ){

        SYNC_TIMER.PER = SYNC_PER - temp;
    }
    else{

        SYNC_TIMER.PER = SYNC_PER + temp;   
    }

    // if( offset > 0 ){

    //     SYNC_TIMER.CNT -= ( abs(offset) * TICKS_PER_MS );
    // }
    // else{

    //     SYNC_TIMER.CNT += ( abs(offset) * TICKS_PER_MS );
    // }
}








PT_THREAD( time_server_thread( pt_t *pt, void *state ) );
PT_THREAD( time_master_thread( pt_t *pt, void *state ) );

static socket_t sock;

static uint32_t local_time;
static uint32_t net_time;
static ip_addr_t master_ip;
// static uint32_t master_uptime;
static uint32_t last_frame_sync;

static ip_addr_t frame_master_ip;

static int16_t filtered_drift;

static uint8_t frame_sync_state;
#define FRAME_SYNC_OFF      0
#define FRAME_SYNC_WAIT     1
#define FRAME_SYNC_MASTER   2
#define FRAME_SYNC_SLAVE    3



int8_t timesync_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

            
    }

    return 0;
}



KV_SECTION_META kv_meta_t time_info_kv[] = {
    { SAPPHIRE_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, &net_time,         0,                      "net_time" },
    { SAPPHIRE_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,        0,                      "net_time_master_ip" },

    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,   0,                 timesync_i8_kv_handler, "gfx_sync_group" },
};


void time_v_init( void ){

    // return;

    // PIXEL_EN_PORT.OUTSET = ( 1 << PIXEL_EN_PIN );
    // PIX_CLK_PORT.DIRSET = ( 1 << PIX_CLK_PIN );
    // PIX_CLK_PORT.OUTCLR = ( 1 << PIX_CLK_PIN );

    // SYNC_TIMER.INTCTRLA |= TC_OVFINTLVL_HI_gc;
    // SYNC_TIMER.CTRLA = TC_CLKSEL_DIV1024_gc;
    // SYNC_TIMER.PER = SYNC_PER;

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }


    frame_sync_state = FRAME_SYNC_WAIT;

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
}

static uint32_t get_sync_group_hash( void ){

    char sync_group[32];
    if( kv_i8_get( __KV__gfx_sync_group, sync_group, sizeof(sync_group) ) < 0 ){

        return 0;
    }

    return hash_u32_string( sync_group );    
}


void time_v_send_frame_sync( wifi_msg_vm_frame_sync_t *sync ){

    // check if we are frame master
    if( frame_sync_state != FRAME_SYNC_MASTER ){

        return;
    }

    // check if we're in a sync group
    if( get_sync_group_hash() == 0 ){

        return;
    }

    // ip_addr_t local_ip;
    // cfg_i8_get( CFG_PARAM_IP_ADDRESS, &local_ip );

    // if( !ip_b_addr_compare( local_ip, frame_master_ip ) ){

    //     return;
    // }

    uint8_t buf[sizeof(time_msg_frame_sync_t) + ( WIFI_DATA_FRAME_SYNC_MAX_DATA * sizeof(int32_t) )];
    time_msg_frame_sync_t *msg = (time_msg_frame_sync_t *)buf;
    int32_t *data = (int32_t *)( msg + 1 );

    msg->magic      = TIME_PROTOCOL_MAGIC;
    msg->version    = TIME_PROTOCOL_VERSION;
    msg->type       = TIME_MSG_FRAME_SYNC;

    msg->sync_group     = get_sync_group_hash();
    msg->frame_number   = sync->frame_number;
    msg->rng_seed       = sync->rng_seed;
    msg->reserved       = 0;
    msg->data_index     = sync->data_index;
    msg->data_count     = sync->data_count;

    memcpy( data, sync->data, ( WIFI_DATA_FRAME_SYNC_MAX_DATA * sizeof(int32_t) ) );

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    sock_i16_sendto( sock, buf, sizeof(buf), &raddr );

    last_frame_sync = tmr_u32_get_system_time_ms();

    // log_v_debug_P( PSTR("sending frame sync #%u"), msg->frame_number );
}

uint32_t time_u32_get_network_time( void ){

    return 0;

    uint32_t adjusted_net_time;

    ATOMIC;

    int32_t elapsed = tmr_u32_elapsed_time_ms( local_time );

    // now adjust for drift
    int32_t drift = ( filtered_drift * elapsed ) / 65536;

    adjusted_net_time = net_time + ( elapsed - drift );

    END_ATOMIC;

    return adjusted_net_time;
}



PT_THREAD( time_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    frame_sync_state = FRAME_SYNC_MASTER;

    THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

    TMR_WAIT( pt, rnd_u16_get_int() >> 5 );

    ip_addr_t local_ip;
    cfg_i8_get( CFG_PARAM_IP_ADDRESS, &local_ip );
    // uint32_t local_ip_u32 = ip_u32_to_int( local_ip );
    frame_master_ip = local_ip;

    
    while(1){

        // listen
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

            if( *type == TIME_MSG_FRAME_SYNC ){

                time_msg_frame_sync_t *msg = (time_msg_frame_sync_t *)magic;

                // check if in a sync group
                if( get_sync_group_hash() == 0 ){

                    continue;
                }

                if( msg->sync_group != get_sync_group_hash() ){

                    continue;
                }

                sock_addr_t raddr;
                sock_v_get_raddr( sock, &raddr );

                if( frame_sync_state == FRAME_SYNC_WAIT ){

                    frame_sync_state = FRAME_SYNC_SLAVE;  
                    frame_master_ip = raddr.ipaddr; 
                    log_v_debug_P( PSTR("assigning frame master: %d.%d.%d.%d"), 
                        raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );                    
                }
                // else if( frame_sync_state == FRAME_SYNC_MASTER ){
                else{

                    if( ( ip_u32_to_int( raddr.ipaddr ) < ip_u32_to_int( frame_master_ip ) ) ||
                        ( ip_b_is_zeroes( frame_master_ip ) ) ){

                        log_v_debug_P( PSTR("assigning new frame master: %d.%d.%d.%d"),
                            raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

                        frame_sync_state = FRAME_SYNC_SLAVE;
                        frame_master_ip = raddr.ipaddr;                    
                    }
                }

                if( frame_sync_state == FRAME_SYNC_MASTER ){

                    continue;
                }


                // sock_addr_t raddr;
                // sock_v_get_raddr( sock, &raddr );

                // if( ( ip_u32_to_int( raddr.ipaddr ) < ip_u32_to_int( frame_master_ip ) ) ||
                //     ( ip_b_is_zeroes( frame_master_ip ) ) ){

                //     log_v_debug_P( PSTR("assigning new frame master: %d.%d.%d.%d"),
                //     raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

                //     frame_master_ip = raddr.ipaddr;                    
                // }
                // else if( !ip_b_addr_compare( raddr.ipaddr, frame_master_ip ) ){

                //     continue;
                // }

                int32_t *data = (int32_t *)( msg + 1 );

                gfx_v_frame_sync( 
                    msg->frame_number, 
                    msg->rng_seed, 
                    msg->data_index, 
                    msg->data_count, 
                    data );

                // log_v_debug_P( PSTR("received frame sync: #%5d"), msg->frame_number );

                // last_frame_sync = tmr_u32_get_system_time_ms();
            }
        }
    }

PT_END( pt );
}

PT_THREAD( time_master_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

    // cfg_i8_get( CFG_PARAM_IP_ADDRESS, &master_ip );
    // cfg_i8_get( CFG_PARAM_IP_ADDRESS, &frame_master_ip );
    
    while(1){

        // if( frame_sync_state == FRAME_SYNC_WAIT ){

        //     TMR_WAIT( pt, 8000 );
        // }

        // if( frame_sync_state == FRAME_SYNC_WAIT ){

        //     log_v_debug_P( PSTR("setting frame master") );
        //     frame_sync_state = FRAME_SYNC_MASTER;
        // }

        // if( frame_sync_state == FRAME_SYNC_MASTER ){


        // }
        // else if( frame_sync_state == FRAME_SYNC_SLAVE ){
            

        // }



        // // check if we haven't received a timestamp in a while
        // if( tmr_u32_elapsed_time_ms( local_time ) > 32000 ){

        //     log_v_debug_P( PSTR("master timeout, resetting us to master") );
        //     cfg_i8_get( CFG_PARAM_IP_ADDRESS, &master_ip );
        // }

        // ip_addr_t local_ip;
        // cfg_i8_get( CFG_PARAM_IP_ADDRESS, &local_ip );

        // // check if we are master
        // if( ip_b_addr_compare( local_ip, master_ip ) ){

        //     filtered_drift = 0xffff;

        //     master_uptime       = tmr_u64_get_system_time_us() / 1000000;

        //     // build sync msg
        //     time_msg_sync_t msg;
        //     msg.magic           = TIME_PROTOCOL_MAGIC;
        //     msg.type            = TIME_MSG_SYNC;
        //     msg.uptime          = master_uptime;

        //     // set up broadcast address
        //     sock_addr_t raddr;
        //     raddr.port = TIME_SERVER_PORT;
        //     raddr.ipaddr = ip_a_addr(255,255,255,255);

        //     // set timestamp at last possible instant and send
        //     uint32_t now = tmr_u32_get_system_time_ms();
        //     net_time += tmr_u32_elapsed_times( local_time, now );

        //     // log_v_debug_P( PSTR("sending net time: %lu"), net_time );

        //     msg.network_time = net_time;

        //     sock_i16_sendto( sock, &msg, sizeof(msg), &raddr );

        //     local_time = now;
        // }   

        // THREAD_YIELD( pt );

        // // check if we haven't received a timestamp in a while
        // if( tmr_u32_elapsed_time_ms( last_frame_sync ) > 32000 ){

        //     log_v_debug_P( PSTR("frame master timeout, resetting us to master") );
        //     cfg_i8_get( CFG_PARAM_IP_ADDRESS, &frame_master_ip );
        // }

        

        TMR_WAIT( pt, 100 );
        THREAD_YIELD( pt );
    }

PT_END( pt );
}




// PT_THREAD( time_server_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
    
//     while(1){

//         // listen
//         THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

//         ATOMIC;
//         uint32_t now = tmr_u32_get_system_time_ms();
//         uint32_t est = time_u32_get_network_time();
//         END_ATOMIC;
    
//         // check if data received
//         if( sock_i16_get_bytes_read( sock ) > 0 ){

//             uint32_t *magic = sock_vp_get_data( sock );

//             if( *magic != TIME_PROTOCOL_MAGIC ){

//                 continue;
//             }

//             uint8_t *type = (uint8_t *)(magic + 1);

//             if( *type == TIME_MSG_SYNC ){

//                 // STROBE;

//                 time_msg_sync_t *msg = (time_msg_sync_t *)magic;

//                 sock_addr_t raddr;
//                 sock_v_get_raddr( sock, &raddr );

//                 bool is_master = ip_b_addr_compare( raddr.ipaddr, master_ip );

//                 // check if this is a better master to sync to
//                 if( ( !is_master ) && 
//                     ( ( msg->uptime > master_uptime ) ||
//                       ( ( msg->uptime == master_uptime ) &&
//                         ( ip_u32_to_int( raddr.ipaddr ) < ip_u32_to_int( master_ip ) ) ) ) ){

//                     master_ip       = raddr.ipaddr;
//                     master_uptime   = msg->uptime;

//                     log_v_debug_P( PSTR("assigning new master: %d.%d.%d.%d"),
//                     raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

//                     filtered_drift = 0xffff;

//                     is_master = TRUE;
//                 }

//                 // check if this message is from our sync master
//                 if( is_master ){

//                     // check validity of master
//                     if( msg->uptime < master_uptime ){

//                         // net time is off by too much, possibly the master rebooted and
//                         // came back before we timed out.

//                         log_v_debug_P( PSTR("master invalid") );

//                         master_ip = ip_a_addr(0,0,0,0);
//                     }
//                     else{

//                         int32_t net_diff = msg->network_time - net_time;
//                         int32_t local_diff = now - local_time;
//                         int32_t diff = local_diff - net_diff;

//                         int16_t drift = ( diff * 65536 ) / net_diff;

//                         if( abs(drift) < 500 ){

//                             if( (uint16_t)filtered_drift == 0xffff ){

//                                 filtered_drift = drift;
//                             }
//                             else{

//                                 uint16_t filter = 8;
//                                 filtered_drift = ( ( (int32_t)filter * (int32_t)drift ) / 128 ) +
//                                                  ( ( ( (int32_t)( 128 - filter ) ) * (int32_t)filtered_drift ) / 128 );
//                             }
//                         }

//                         net_time        = msg->network_time;   
//                         local_time      = now;
//                         master_uptime   = msg->uptime;

//                         log_v_debug_P( PSTR("received net time: %lu est: %lu net: %ld local: %ld drift: %d filt: %d"), net_time, est, net_diff, local_diff, drift, filtered_drift );
//                     }
//                 }
//             }
//             else if( *type == TIME_MSG_FRAME_SYNC ){

//                 time_msg_frame_sync_t *msg = (time_msg_frame_sync_t *)magic;

//                 if( msg->sync_group != get_sync_group_hash() ){

//                     continue;
//                 }

//                 sock_addr_t raddr;
//                 sock_v_get_raddr( sock, &raddr );

//                 if( ( ip_u32_to_int( raddr.ipaddr ) < ip_u32_to_int( frame_master_ip ) ) ||
//                     ( ip_b_is_zeroes( frame_master_ip ) ) ){

//                     log_v_debug_P( PSTR("assigning new frame master: %d.%d.%d.%d"),
//                     raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

//                     frame_master_ip = raddr.ipaddr;                    
//                 }
//                 else if( !ip_b_addr_compare( raddr.ipaddr, frame_master_ip ) ){

//                     continue;
//                 }

//                 int32_t *data = (int32_t *)( msg + 1 );

//                 vm_v_frame_sync( 
//                     msg->frame_number, 
//                     msg->rng_seed, 
//                     msg->data_index, 
//                     msg->data_count, 
//                     data );

//                 // log_v_debug_P( PSTR("received frame sync: #%5d"), msg->frame_number );

//                 last_frame_sync = tmr_u32_get_system_time_ms();
//             }
//         }
//     }

// PT_END( pt );
// }

// PT_THREAD( time_master_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
    
//     THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

//     cfg_i8_get( CFG_PARAM_IP_ADDRESS, &master_ip );
//     cfg_i8_get( CFG_PARAM_IP_ADDRESS, &frame_master_ip );
    
//     while(1){

//         // check if we haven't received a timestamp in a while
//         if( tmr_u32_elapsed_time_ms( local_time ) > 32000 ){

//             log_v_debug_P( PSTR("master timeout, resetting us to master") );
//             cfg_i8_get( CFG_PARAM_IP_ADDRESS, &master_ip );
//         }

//         ip_addr_t local_ip;
//         cfg_i8_get( CFG_PARAM_IP_ADDRESS, &local_ip );

//         // check if we are master
//         if( ip_b_addr_compare( local_ip, master_ip ) ){

//             filtered_drift = 0xffff;

//             master_uptime       = tmr_u64_get_system_time_us() / 1000000;

//             // build sync msg
//             time_msg_sync_t msg;
//             msg.magic           = TIME_PROTOCOL_MAGIC;
//             msg.type            = TIME_MSG_SYNC;
//             msg.uptime          = master_uptime;

//             // set up broadcast address
//             sock_addr_t raddr;
//             raddr.port = TIME_SERVER_PORT;
//             raddr.ipaddr = ip_a_addr(255,255,255,255);

//             // set timestamp at last possible instant and send
//             uint32_t now = tmr_u32_get_system_time_ms();
//             net_time += tmr_u32_elapsed_times( local_time, now );

//             // log_v_debug_P( PSTR("sending net time: %lu"), net_time );

//             msg.network_time = net_time;

//             sock_i16_sendto( sock, &msg, sizeof(msg), &raddr );

//             local_time = now;
//         }   

//         THREAD_YIELD( pt );

//         // check if we haven't received a timestamp in a while
//         if( tmr_u32_elapsed_time_ms( last_frame_sync ) > 32000 ){

//             log_v_debug_P( PSTR("frame master timeout, resetting us to master") );
//             cfg_i8_get( CFG_PARAM_IP_ADDRESS, &frame_master_ip );
//         }

        

//         TMR_WAIT( pt, 8000 );
//     }

// PT_END( pt );
// }


#endif
#endif