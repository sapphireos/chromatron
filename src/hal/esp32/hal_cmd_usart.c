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

#include "cpu.h"
#include "system.h"
#include "threading.h"

#include "cmd_usart.h"

#include "hal_cmd_usart.h"

#include "driver/uart.h"

#define RX_FIFO_SIZE 256

static uint8_t rx_fifo[RX_FIFO_SIZE];
static uint16_t rx_ins;
static uint16_t rx_ext;
static uint16_t rx_size;

static int16_t read_char( void ){

    if( rx_size >= cnt_of_array(rx_fifo) ){

        return -1;
    }

    uint8_t temp;
    if( uart_read_bytes( HAL_CMD_UART, &temp, 1, 0 ) != 1 ){

        return -2;
    }

    rx_fifo[rx_ins] = temp;
    rx_ins++;
    if( rx_ins >= cnt_of_array(rx_fifo) ){

        rx_ins = 0;
    }

    rx_size++;

    return temp;
}

PT_THREAD( uart_receiver_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );  
    
    while(1){
        
        THREAD_WAIT_WHILE( pt, read_char() < 0 );

        while( read_char() >= 0 );

        THREAD_YIELD( pt );
    }

PT_END( pt );
}

void cmd_usart_v_set_baud( baud_t baud ){

}

bool cmd_usart_b_received_char( void ){

    return cmd_usart_u16_rx_size() > 0;
}

void cmd_usart_v_send_char( uint8_t data ){

    uart_write_bytes( HAL_CMD_UART, (const char *) &data, 1 );
}

void cmd_usart_v_send_data( const uint8_t *data, uint16_t len ){

    uart_write_bytes( HAL_CMD_UART, (const char *) data, len );
}


int16_t cmd_usart_i16_get_char( void ){

    if( cmd_usart_u16_rx_size() == 0 ){

        return -1;
    }

    uint8_t temp = rx_fifo[rx_ext];
    rx_ext++;
    if( rx_ext >= cnt_of_array(rx_fifo) ){

        rx_ext = 0;
    }
    rx_size--;

    return temp;
}

uint8_t cmd_usart_u8_get_data( uint8_t *data, uint8_t len ){

    uint8_t size = cmd_usart_u16_rx_size();

    if( len > size ){

        len = size;
    }

    size = len;

    while( len > 0 ){

        *data = cmd_usart_i16_get_char();
        data++;

        len--;
    }

    return size;
}

uint16_t cmd_usart_u16_rx_size( void ){

    return rx_size;
}

void cmd_usart_v_flush( void ){

}


void hal_cmd_usart_v_init( void ){

    thread_t_create( uart_receiver_thread,
                     PSTR("cmd_uart_receiver"),
                     0,
                     0 );

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config( HAL_CMD_UART, &uart_config );
    uart_set_pin( HAL_CMD_UART, HAL_CMD_TXD, HAL_CMD_RXD, -1, -1 );
    uart_driver_install( HAL_CMD_UART, HAL_CMD_RX_BUF_SIZE, 0, 0, NULL, 0 );
}



    