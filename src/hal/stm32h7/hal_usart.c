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

#include "system.h"
#include "timers.h"


#include "hal_usart.h"

void usart_v_init( USART_t *usart ){

    // enable clock
    // __HAL_RCC_USART1_CLK_ENABLE();

    // usart->Instance = HAL_USART;

	// defaults: 115200, 8N1
    usart->Init.BaudRate                = 115200;
    usart->Init.WordLength              = UART_WORDLENGTH_8B;
    usart->Init.StopBits                = UART_STOPBITS_1;
    usart->Init.Parity                  = UART_PARITY_NONE;
    usart->Init.Mode                    = UART_MODE_TX_RX;
    usart->Init.HwFlowCtl               = UART_HWCONTROL_NONE;
    usart->Init.OverSampling            = UART_OVERSAMPLING_16;
    usart->Init.OneBitSampling          = UART_ONE_BIT_SAMPLE_DISABLE;
    usart->Init.FIFOMode                = UART_FIFOMODE_DISABLE; 
    usart->AdvancedInit.AdvFeatureInit  = UART_ADVFEATURE_NO_INIT;

    // usart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;

    if (HAL_UART_Init(usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }
}

void usart_v_set_baud( USART_t *usart, baud_t baud ){

	usart->Init.BaudRate = baud;
    
	if (HAL_UART_Init(usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }
}

void usart_v_set_double_speed( USART_t *usart, bool clk2x ){

	uint32_t baud = usart->Init.BaudRate;

	if( clk2x ){

		usart->Init.BaudRate = baud * 2;	
	}
	else{

		usart->Init.BaudRate = baud;			
	}
	
	if (HAL_UART_Init(usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    usart->Init.BaudRate = baud;
}

void usart_v_send_byte( USART_t *usart, uint8_t data ){

	HAL_UART_Transmit( usart, &data, sizeof(data), 100 );
}

void usart_v_send_data( USART_t *usart, const uint8_t *data, uint16_t len ){

	HAL_UART_Transmit( usart, (uint8_t *)data, len, 100 );
}

int16_t usart_i16_get_byte( USART_t *usart ){

    if( __HAL_UART_GET_FLAG( usart, UART_FLAG_RXFNE ) ){

        return usart->Instance->RDR;
    }

	return -1;
}

uint8_t usart_u8_bytes_available( USART_t *usart ){

    if( __HAL_UART_GET_FLAG( usart, UART_FLAG_RXFNE ) ){

        return 1;
    }

	return 0;
}
