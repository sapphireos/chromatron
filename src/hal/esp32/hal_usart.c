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


#include "system.h"
#include "timers.h"

#include "hal_usart.h"

#include "driver/uart.h"

// Setup UART buffered IO with event queue
#define UART_RX_BUFFER_SIZE 2048
#define UART_TX_BUFFER_SIZE 0


void usart_v_init( uint8_t channel ){
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
    };

    ESP_ERROR_CHECK( uart_param_config( channel, &uart_config ) );

    // tx, rx, rts, cts
    ESP_ERROR_CHECK( uart_set_pin( channel, 9, 10, -1, -1 ) );

    // Install UART driver using an event queue here
    ESP_ERROR_CHECK( uart_driver_install(
                        channel, 
                        UART_RX_BUFFER_SIZE,
                        UART_TX_BUFFER_SIZE, 
                        0, 
                        0, 
                        0 ) );
}

void usart_v_set_baud( uint8_t channel, baud_t baud ){

    uart_set_baudrate( channel, baud );
}

void usart_v_send_byte( uint8_t channel, uint8_t data ){

    uart_write_bytes( channel, &data, sizeof(data) );
}

void usart_v_send_data( uint8_t channel, const uint8_t *data, uint16_t len ){

    uart_write_bytes( channel, data, len );
}

int16_t usart_i16_get_byte( uint8_t channel ){

    uint8_t byte;

    if( uart_read_bytes( channel, &byte, sizeof(byte), 0 ) != 1 ){

        return -1;
    }

    return byte;
}

uint8_t usart_u8_bytes_available( uint8_t channel ){

    size_t size = 0;

    uart_get_buffered_data_len( channel, &size );

    if( size > 255 ){

        size = 255;
    }

    return size;
}

