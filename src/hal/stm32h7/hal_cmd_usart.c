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

#include "hal_cmd_usart.h"
#include "netmsg.h"
#include "sockets.h"
#include "threading.h"
#include "crc.h"
#include "timers.h"
#include "logging.h"
#include "hal_io.h"

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

#ifdef PRINTF_SUPPORT
#include <stdio.h>
#endif

PT_THREAD( serial_cmd_thread( pt_t *pt, void *state ) );

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

#ifndef ENABLE_USB
static UART_HandleTypeDef cmd_usart;

static volatile uint8_t rx_buf[HAL_CMD_USART_RX_BUF_SIZE];
static volatile uint8_t rx_ins;
static volatile uint8_t rx_ext;
static volatile uint8_t rx_size;

// ISR(USART3_IRQHandler){
ISR(UART4_IRQHandler){

    while( __HAL_UART_GET_FLAG( &cmd_usart, UART_FLAG_RXNE ) ){

        // HAL_UART_Receive( &cmd_usart, (uint8_t *)&rx_buf[rx_ins], 1, 5 );

        rx_buf[rx_ins] = cmd_usart.Instance->RDR;

        rx_ins++;

        if( rx_ins >= cnt_of_array(rx_buf) ){

            rx_ins = 0;
        }

        rx_size++;
    }

    while( __HAL_UART_GET_FLAG( &cmd_usart, UART_FLAG_ORE ) ){

        __HAL_UART_CLEAR_IT( &cmd_usart, UART_FLAG_ORE );    
    }
}
#endif

int8_t cmd_usart_i8_get_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask ){

    *subnet = ip_a_addr(240,1,2,3);
    *subnet_mask = ip_a_addr(255,255,255,255);

    return 0;
}

int8_t cmd_usart_i8_transmit( netmsg_t msg ){

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

    #ifndef ENABLE_USB
    // enable clock
    // __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();

    // init IO pins
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin = CMD_USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    // GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(CMD_USART_RX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CMD_USART_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    // GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(CMD_USART_TX_GPIO_Port, &GPIO_InitStruct);

    // initialize command usart
    cmd_usart.Instance = HAL_CMD_USART;
    cmd_usart.Init.BaudRate = 115200;
    cmd_usart.Init.WordLength = UART_WORDLENGTH_8B;
    cmd_usart.Init.StopBits = UART_STOPBITS_1;
    cmd_usart.Init.Parity = UART_PARITY_NONE;
    cmd_usart.Init.Mode = UART_MODE_TX_RX;
    cmd_usart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    cmd_usart.Init.OverSampling = UART_OVERSAMPLING_16;
    cmd_usart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    cmd_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&cmd_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    __HAL_UART_ENABLE_IT( &cmd_usart, UART_IT_RXNE );

    // HAL_NVIC_SetPriority( USART3_IRQn, 0, 0 );
    // HAL_NVIC_EnableIRQ( USART3_IRQn );
    HAL_NVIC_SetPriority( UART4_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( UART4_IRQn );
    #endif
    
    // create serial thread
    thread_t_create( serial_cmd_thread,
                     PSTR("serial_cmd"),
                     0,
                     0 );

}

void cmd_usart_v_set_baud( baud_t baud ){

}

bool cmd_usart_b_received_char( void ){

    return cmd_usart_u16_rx_size() > 0;
}

void cmd_usart_v_send_char( uint8_t data ){

    #ifdef ENABLE_USB
    usb_v_send_char( data );
    #else
    HAL_UART_Transmit( &cmd_usart, &data, sizeof(data), 100 );
    #endif
}

void cmd_usart_v_send_data( const uint8_t *data, uint16_t len ){

    #ifdef ENABLE_USB
    usb_v_send_data( data, len );
    #else
    HAL_UART_Transmit( &cmd_usart, (uint8_t *)data, len, 100 );
    #endif
}


int16_t cmd_usart_i16_get_char( void ){

    if( cmd_usart_u16_rx_size() == 0 ){

        return -1;
    }

    #ifdef ENABLE_USB

    return usb_i16_get_char();

    #else
    uint8_t temp = rx_buf[rx_ext];
    rx_ext++;

    if( rx_ext >= cnt_of_array(rx_buf) ){

        rx_ext = 0;
    }

    ATOMIC;
    rx_size--;
    END_ATOMIC;

    return temp;
    #endif
}

uint8_t cmd_usart_u8_get_data( uint8_t *data, uint8_t len ){

    uint8_t rx_size = cmd_usart_u16_rx_size();

    if( len > rx_size ){

        len = rx_size;
    }

    rx_size = len;

    while( len > 0 ){

        *data = cmd_usart_i16_get_char();
        data++;

        len--;
    }

    return rx_size;
}

uint16_t cmd_usart_u16_rx_size( void ){

    #ifdef ENABLE_USB

    return usb_u16_rx_size();

    #else
    ATOMIC;
    uint16_t temp = rx_size;
    END_ATOMIC;

    return temp;
    #endif
}

void cmd_usart_v_flush( void ){

    #ifdef ENABLE_USB

    usb_v_flush();

    #else
    ATOMIC;
    rx_ins = rx_ext;
    rx_size = 0;
    END_ATOMIC;
    #endif
}

static netmsg_t netmsg;
static uint16_t idx;
static uint16_t count;

PT_THREAD( serial_cmd_thread( pt_t *pt, void *state ) )
{
    netmsg_state_t *nm_state = 0;

PT_BEGIN( pt );

    while(1){

        THREAD_YIELD( pt );

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

        // check for empty message
        if( header.data_len == 0 ){
            // useful for comms check

            cmd_usart_v_send_char( CMD_USART_UDP_ACK ); 
            log_v_debug_P( PSTR("empty") );
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

            log_v_debug_P( PSTR("data alloc fail") );     

            goto cleanup;
        }

        // set up address info
        nm_state->laddr.port   = header.rport;
        nm_state->laddr.ipaddr = ip_a_addr(240,1,2,3);
        nm_state->raddr.port   = header.lport;
        nm_state->raddr.ipaddr = ip_a_addr(240,1,2,3);

        idx = 0;
        count = header.data_len;


        while( count > 0 ){

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

            uint16_t rx_data = cmd_usart_u16_rx_size();

            if( rx_data > count ){

                rx_data = count;
            }

            while( rx_data > 0 ){
                
                *buf = cmd_usart_i16_get_char();
                buf++;
                idx++;
                count--;
                rx_data--;
            }
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

