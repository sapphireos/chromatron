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


#include "cpu.h"
#include "system.h"

#include "usb_intf.h"
#include "netmsg.h"
#include "sockets.h"
#include "threading.h"
#include "crc.h"
#include "timers.h"
#include "logging.h"

#include "cmd_usart.h"
#include "hal_cmd_usart.h"


#ifdef PRINTF_SUPPORT
#include <stdio.h>
#endif

#ifdef ENABLE_USB_UDP_TRANSPORT
PT_THREAD( serial_udp_thread( pt_t *pt, void *state ) );
#endif

#ifdef PRINTF_SUPPORT
static int uart_putchar( char c, FILE *stream );
static FILE uart_stdout = FDEV_SETUP_STREAM( uart_putchar, NULL, _FDEV_SETUP_WRITE );

static int uart_putchar( char c, FILE *stream )
{
    if( c == '\n' ){

        cmd_usart_v_send_char( '\r' );
    }

    cmd_usart_v_send_char( c );

    return 0;
}
#endif


int8_t cmd_usart_i8_get_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask ){

    #ifdef ENABLE_USB_UDP_TRANSPORT

    *subnet = ip_a_addr(240,1,2,3);
    *subnet_mask = ip_a_addr(255,255,255,255);

    return NETMSG_ROUTE_AVAILABLE;

    #else

    return NETMSG_ROUTE_NOT_AVAILABLE;
    #endif
}

int8_t cmd_usart_i8_transmit( netmsg_t msg ){

    #ifdef ENABLE_USB_UDP_TRANSPORT
    netmsg_state_t *state = netmsg_vp_get_state( msg );

    cmd_usart_v_send_char( CMD_USART_UDP_SOF );
    cmd_usart_v_send_char( CMD_USART_VERSION );

    cmd_usart_udp_header_t header;
    header.lport        = state->laddr.port;
    header.rport        = state->raddr.port;
    header.data_len     = mem2_u16_get_size( state->data_handle );
    header.header_crc   = 0;

    header.header_crc   = crc_u16_block( (uint8_t *)&header, sizeof(header) );

    // log_v_debug_P(PSTR("SEND: %d"), header.data_len);

    cmd_usart_v_send_data( (uint8_t *)&header, sizeof(header) );

    uint16_t crc = crc_u16_block( mem2_vp_get_ptr( state->data_handle ), header.data_len );
    cmd_usart_v_send_data( mem2_vp_get_ptr( state->data_handle ), header.data_len );

    cmd_usart_v_send_data( (uint8_t *)&crc, sizeof(crc) );
    #endif

    return NETMSG_TX_OK_RELEASE;
}

// first entry is loopback interface
ROUTING_TABLE routing_table_entry_t cmd_usart_route = {
    cmd_usart_i8_get_route,
    cmd_usart_i8_transmit,
    0,
    0
};

void cmd_usart_v_init( void ){

    #ifdef ENABLE_USB_UDP_TRANSPORT
    
    hal_cmd_usart_v_init();

    // create serial thread
    thread_t_create( serial_udp_thread,
                     PSTR("serial_udp"),
                     0,
                     0 );

    #endif
}

#ifdef ENABLE_USB_UDP_TRANSPORT

static netmsg_t netmsg;
static uint16_t idx;
static uint16_t count;

PT_THREAD( serial_udp_thread( pt_t *pt, void *state ) )
{

    netmsg_state_t *nm_state = 0;

PT_BEGIN( pt );

    while(1){

        THREAD_YIELD( pt );

        // wait until we have enough space to receive a packet:
        THREAD_WAIT_WHILE( pt, mem2_u16_get_free() < CMD_USART_MAX_PACKET_LEN );

        count = 0;
        idx = 0;
        netmsg = -1;

        // wait for a frame start indicator
        THREAD_WAIT_WHILE( pt, cmd_usart_i16_get_char() != CMD_USART_UDP_SOF );

        // wait for version
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + CMD_USART_TIMEOUT_MS );
        THREAD_WAIT_WHILE( pt, ( cmd_usart_u16_rx_size() < sizeof(uint8_t) ) &&
                               ( thread_b_alarm_set() ) );

        // check for timeout
        if( thread_b_alarm() ){

            cmd_usart_v_send_char( CMD_USART_UDP_NAK );
            log_v_debug_P( PSTR("timeout") );

            log_v_debug_P( PSTR("%d %d"), cmd_usart_u16_rx_size(), thread_u8_get_run_cause() );
            
            goto cleanup;
        }

        if( cmd_usart_i16_get_char() != CMD_USART_VERSION ){

            cmd_usart_v_send_char( CMD_USART_UDP_NAK );
            log_v_debug_P( PSTR("wrong version") );
            
            goto cleanup;
        }

        // wait for header
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + CMD_USART_TIMEOUT_MS );
        THREAD_WAIT_WHILE( pt, ( cmd_usart_u16_rx_size() < sizeof(cmd_usart_udp_header_t) ) &&
                               ( thread_b_alarm_set() ) );

        // check for timeout
        if( thread_b_alarm() ){

            cmd_usart_v_send_char( CMD_USART_UDP_NAK );
            log_v_debug_P( PSTR("timeout") );
            
            goto cleanup;
        }

        // copy header
        cmd_usart_udp_header_t header;
        if( cmd_usart_u8_get_data( (uint8_t *)&header, sizeof(header) ) != sizeof(header) ){

            cmd_usart_v_send_char( CMD_USART_UDP_NAK );
            log_v_debug_P( PSTR("bad header count") );
            
            goto cleanup;
        }

        uint16_t temp_header_crc = header.header_crc;
        header.header_crc = 0;
        uint16_t header_crc = crc_u16_block( (uint8_t *)&header, sizeof(header) );
        
        // check header
        if( ( header.data_len > CMD_USART_MAX_PACKET_LEN ) ||
            ( header_crc != temp_header_crc ) ){

            cmd_usart_v_send_char( CMD_USART_UDP_NAK );
            log_v_debug_P( PSTR("bad header %x != %x"), header_crc, temp_header_crc );
            
            goto cleanup;
        }

        // log_v_debug_P(PSTR("RECV: %d"), header.data_len);

        // check for empty message
        if( header.data_len == 0 ){
            // useful for comms check

            cmd_usart_v_send_char( CMD_USART_UDP_ACK ); 
            continue;
        }

        // allocate netmsg
        netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );        

        if( netmsg < 0 ){

            log_v_debug_P( PSTR("netmsg alloc fail") );
        }

        nm_state = netmsg_vp_get_state( netmsg );

        // allocate data handle
        nm_state->data_handle = mem2_h_alloc2( header.data_len, MEM_TYPE_SOCKET_BUFFER );

        if( nm_state->data_handle < 0 ){

            log_v_debug_P( PSTR("data alloc fail d:%d f:%d r:%d"), mem2_u16_get_dirty(), mem2_u16_get_free(), header.data_len );     

            goto cleanup;
        }

        // set up address info
        nm_state->laddr.port   = header.rport;
        nm_state->laddr.ipaddr = ip_a_addr(240,1,2,3);
        nm_state->raddr.port   = header.lport;
        nm_state->raddr.ipaddr = ip_a_addr(240,1,2,3);

        idx = 0;
        count = header.data_len;


        while( idx < count ){

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + CMD_USART_TIMEOUT_MS );
            THREAD_WAIT_WHILE( pt, ( cmd_usart_u16_rx_size() == 0 ) &&
                                   ( thread_b_alarm_set() ) );

            // check for timeout
            if( thread_b_alarm() ){

                cmd_usart_v_send_char( CMD_USART_UDP_NAK );
                log_v_debug_P( PSTR("timeout") );
                
                goto cleanup;
            }

            netmsg_state_t *nm_state = netmsg_vp_get_state( netmsg );
            uint8_t *buf = mem2_vp_get_ptr( nm_state->data_handle ) + idx;

            *buf = cmd_usart_i16_get_char();
            idx++;
        }

        // wait for CRC
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + CMD_USART_TIMEOUT_MS );
        THREAD_WAIT_WHILE( pt, ( cmd_usart_u16_rx_size() < sizeof(uint16_t) ) &&
                               ( thread_b_alarm_set() ) );

        if( cmd_usart_u16_rx_size() < sizeof(uint16_t) ){

            // send NAK
            cmd_usart_v_send_char( CMD_USART_UDP_NAK );

            log_v_debug_P( PSTR("crc timeout") );   
            goto cleanup;
        }

        uint16_t crc = 0;
        if( cmd_usart_u8_get_data( (uint8_t *)&crc, sizeof(crc) ) != sizeof(crc) ){

            // send NAK
            cmd_usart_v_send_char( CMD_USART_UDP_NAK );

            log_v_debug_P( PSTR("bad crc len") );   

            goto cleanup;
        }

        // check CRC
        nm_state = netmsg_vp_get_state( netmsg );
        uint16_t data_crc = crc_u16_block( mem2_vp_get_ptr( nm_state->data_handle ),
                                           mem2_u16_get_size( nm_state->data_handle ) );
        if(  data_crc != crc ){

            // send NAK
            cmd_usart_v_send_char( CMD_USART_UDP_NAK );

            log_v_debug_P( PSTR("bad crc %x != %x"), data_crc, crc );   

            goto cleanup;
        }

        // send ACK
        cmd_usart_v_send_char( CMD_USART_UDP_ACK );

        while(1){

            nm_state = netmsg_vp_get_state( netmsg );

            // check if port is busy
            if( !sock_b_port_busy( nm_state->laddr.port ) ){

                break;
            }

            THREAD_YIELD( pt );
        }

        // receive message
        netmsg_v_receive( netmsg );
        netmsg = -1;

        // skip clean up, netmsg will free memory
        continue;

cleanup:
        cmd_usart_v_flush();
        log_v_debug_P( PSTR("cleanup") );    

        if( netmsg > 0 ){ 
            netmsg_v_release( netmsg );

            netmsg = -1;
        }

    }   

PT_END( pt );
}
#endif