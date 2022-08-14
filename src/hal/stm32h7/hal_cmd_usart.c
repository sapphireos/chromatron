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

void hal_cmd_usart_v_init( void ){

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
