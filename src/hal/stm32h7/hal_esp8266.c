// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "keyvalue.h"

#include "hal_usart.h"
#include "hal_io.h"

#include "hal_esp8266.h"
#include "wifi_cmd.h"

#include "esp8266.h"


static uint32_t current_tx_bytes;
static uint32_t current_rx_bytes;

static USART_t wifi_usart;

static bool normal_mode;
// static uint16_t timeouts;

// KV_SECTION_META kv_meta_t hal_wifi_kv[] = {
//     { SAPPHIRE_TYPE_UINT16,      0, KV_FLAGS_READ_ONLY, &timeouts, 0,   "wifi_hal_timeouts" },
// };

PT_THREAD( hal_wifi_thread( pt_t *pt, void *state ) );



void hal_wifi_v_init( void ){

    // enable clocks
    #ifdef BOARD_CHROMATRONX
    __HAL_RCC_UART8_CLK_ENABLE();
    #else
    __HAL_RCC_UART4_CLK_ENABLE();
    #endif
    
    wifi_usart.Instance                 = WIFI_USART;
    wifi_usart.Init.BaudRate            = 115200;
    wifi_usart.Init.WordLength          = UART_WORDLENGTH_8B;
    wifi_usart.Init.StopBits            = UART_STOPBITS_1;
    wifi_usart.Init.Parity              = UART_PARITY_NONE;
    wifi_usart.Init.Mode                = UART_MODE_TX_RX;
    wifi_usart.Init.HwFlowCtl           = UART_HWCONTROL_NONE;
    wifi_usart.Init.OverSampling        = UART_OVERSAMPLING_16;
    wifi_usart.Init.OneBitSampling      = UART_ONE_BIT_SAMPLE_DISABLE;
    // wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;

    if (HAL_UART_Init( &wifi_usart ) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }
        
    // set up IO
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin         = WIFI_RST_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLUP;
    HAL_GPIO_Init(WIFI_RST_GPIO_Port, &GPIO_InitStruct);
    

    GPIO_InitStruct.Pin         = WIFI_PD_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_PD_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_CTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_CTS_GPIO_Port, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin         = WIFI_BOOT_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_BOOT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_RTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RTS_Port, &GPIO_InitStruct);

    // hold module in reset
    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_RESET);


    thread_t_create( hal_wifi_thread,
                     PSTR("hal_wifi"),
                     0,
                     0 );
}

PT_THREAD( hal_wifi_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // static wifi_data_header_t *header;


    // // copy firmware to file
    // // ffs_fw_i32_write( 2, 0, wifi_firmware, sizeof(wifi_firmware) );

    // rx_dma_buffer[0] = WIFI_COMM_IDLE;

    // while(1){

    //     THREAD_YIELD( pt );

    //     THREAD_WAIT_WHILE( pt, !normal_mode );

    //     THREAD_WAIT_WHILE( pt, rx_dma_buffer[0] == WIFI_COMM_IDLE );

    //     if( rx_dma_buffer[0] == WIFI_COMM_QUERY_READY ){

    //         // set control byte for main ESP thread
    //         control_byte = rx_dma_buffer[0];

    //         rx_dma_buffer[0] = WIFI_COMM_IDLE;
    //     }
    //     else if( rx_dma_buffer[0] == WIFI_COMM_DATA ){

    //         thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_THREAD_TIMEOUT );
    //         THREAD_WAIT_WHILE( pt, buffer_busy && thread_b_alarm_set() );

    //         // check for timeout
    //         if( thread_b_alarm() ){

    //             trace_printf("timeout1\r\n");

    //             timeouts++;
    //             THREAD_RESTART( pt );
    //         }

    //         thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_THREAD_TIMEOUT );
    //         THREAD_WAIT_WHILE( pt, ( get_dma_bytes() < ( sizeof(wifi_data_header_t) + 1 ) ) &&
    //                                ( thread_b_alarm_set() ) );

    //         // check for timeout
    //         if( thread_b_alarm() ){

    //             trace_printf("timeout2\r\n");

    //             timeouts++;
    //             THREAD_RESTART( pt );
    //         }

    //         header = (wifi_data_header_t *)&rx_dma_buffer[1];

    //         thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_THREAD_TIMEOUT );
    //         THREAD_WAIT_WHILE( pt, ( get_dma_bytes() < ( sizeof(wifi_data_header_t) + 1 + header->len ) ) &&
    //                                ( thread_b_alarm_set() ) );            
            
    //         // check for timeout
    //         if( thread_b_alarm() ){

    //             trace_printf("timeout3\r\n");

    //             timeouts++;
    //             THREAD_RESTART( pt );
    //         }

    //         current_rx_bytes += sizeof(wifi_data_header_t) + 1 + header->len;

    //         // copy to process buffer
    //         memcpy( rx_buf, rx_dma_buffer, sizeof(rx_buf) );

    //         // clear control byte
    //         rx_dma_buffer[0] = WIFI_COMM_IDLE;

    //         // signal ESP our receive buffer is ready
    //         hal_wifi_v_set_rx_ready();

    //         buffer_busy = TRUE;;

    //         // set control byte for wifi process
    //         control_byte = WIFI_COMM_DATA;

    //         // signal thread
    //         thread_v_signal( WIFI_SIGNAL );
    //     }   
    // }

PT_END( pt );
}


void hal_wifi_v_reset( void ){

}

void hal_wifi_v_usart_send_char( uint8_t b ){

    HAL_UART_Transmit( &wifi_usart, &b, sizeof(b), 100 );

    current_tx_bytes += 1;
}

void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len ){

    HAL_UART_Transmit( &wifi_usart, (uint8_t *)data, len, 100 );

    current_tx_bytes += len;
}

int16_t hal_wifi_i16_usart_get_char( void ){

	if( __HAL_UART_GET_FLAG( &wifi_usart, UART_FLAG_RXNE ) ){

        return wifi_usart.Instance->RDR;
    }

    return -1;
}

int16_t hal_wifi_i16_usart_get_char_timeout( uint32_t timeout ){

    uint32_t start_time = tmr_u32_get_system_time_us();

    while( !hal_wifi_b_usart_rx_available() ){

        if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

            return -1;
        }
    }

    return wifi_usart.Instance->RDR;
}

bool hal_wifi_b_usart_rx_available( void ){

    if( __HAL_UART_GET_FLAG( &wifi_usart, UART_FLAG_RXNE ) ){

        return TRUE;
    }

    return FALSE;
}

int8_t hal_wifi_i8_usart_receive( uint8_t *buf, uint16_t len, uint32_t timeout ){

    uint32_t start_time = tmr_u32_get_system_time_us();

    while( len > 0 ){
        while( !hal_wifi_b_usart_rx_available() ){

            if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

                return -1;
            }
        }
        *buf = (uint8_t)hal_wifi_i16_usart_get_char();
        buf++;
        len--;
    }

    return 0;
}

void hal_wifi_v_usart_flush( void ){

    __HAL_UART_CLEAR_FLAG( &wifi_usart, 0xffffffff );

	BUSY_WAIT( hal_wifi_i16_usart_get_char() >= 0 );
}

void hal_wifi_v_usart_set_baud( baud_t baud ){

    wifi_usart.Init.BaudRate = baud;
    
    if (HAL_UART_Init(&wifi_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }
}

int16_t hal_wifi_i16_rx_data_received( void ){

    return -1;
}	


// bool hal_wifi_b_comm_ready( void ){

//     return FALSE;
// }

uint32_t hal_wifi_u32_get_rx_bytes( void ){

	uint32_t temp = current_rx_bytes;

	current_rx_bytes = 0;

	return temp;
}

uint32_t hal_wifi_u32_get_tx_bytes( void ){

	uint32_t temp = current_tx_bytes;

	current_tx_bytes = 0;

	return temp;
}

bool hal_wifi_b_read_rts( void ){

    if( HAL_GPIO_ReadPin( WIFI_RTS_Port, WIFI_RTS_Pin ) == GPIO_PIN_RESET ){

        return false;
    }

    return true;
}

// void hal_wifi_v_set_cts( bool value ){

//     if( value ){

//         HAL_GPIO_WritePin(WIFI_CTS_GPIO_Port, WIFI_CTS_Pin, GPIO_PIN_SET);        
//     }
//     else{

//         HAL_GPIO_WritePin(WIFI_CTS_GPIO_Port, WIFI_CTS_Pin, GPIO_PIN_RESET);
//     }
// }

// reset:
// PD transition low to high

// boot mode:
// low = ROM bootloader
// high = normal execution

void hal_wifi_v_enter_boot_mode( void ){

    normal_mode = FALSE;

    GPIO_InitTypeDef GPIO_InitStruct;

    // set up IO
    GPIO_InitStruct.Pin         = WIFI_PD_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_PD_GPIO_Port, &GPIO_InitStruct);

    // hold module in reset
    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_RESET);

    _delay_ms(WIFI_RESET_DELAY_MS);


    GPIO_InitStruct.Pin         = WIFI_RTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RTS_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_BOOT_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_BOOT_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin         = WIFI_CTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_CTS_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_CTS_GPIO_Port, WIFI_CTS_Pin, GPIO_PIN_RESET);
    
    // re-init uart
    GPIO_InitStruct.Pin = WIFI_RXD_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    #ifdef BOARD_CHROMATRONX
    GPIO_InitStruct.Alternate = GPIO_AF8_UART8;
    #else
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    #endif
    HAL_GPIO_Init(WIFI_RXD_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = WIFI_TXD_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    #ifdef BOARD_CHROMATRONX
    GPIO_InitStruct.Alternate = GPIO_AF8_UART8;
    #else
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    #endif
    HAL_GPIO_Init(WIFI_TXD_GPIO_Port, &GPIO_InitStruct);

    wifi_usart.Init.BaudRate = 115200;
    wifi_usart.Init.WordLength = UART_WORDLENGTH_8B;
    wifi_usart.Init.StopBits = UART_STOPBITS_1;
    wifi_usart.Init.Parity = UART_PARITY_NONE;
    wifi_usart.Init.Mode = UART_MODE_TX_RX;
    wifi_usart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    wifi_usart.Init.OverSampling = UART_OVERSAMPLING_16;
    wifi_usart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    #ifdef BOARD_CHROMATRONX
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    #else
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    #endif
    wifi_usart.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
    if (HAL_UART_Init(&wifi_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    hal_wifi_v_usart_flush();

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_SET);

    _delay_ms(WIFI_RESET_DELAY_MS);
    _delay_ms(WIFI_RESET_DELAY_MS);
    _delay_ms(WIFI_RESET_DELAY_MS);

    // return to inputs
    GPIO_InitStruct.Pin         = WIFI_BOOT_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_BOOT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_CTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_CTS_GPIO_Port, &GPIO_InitStruct);
}

void hal_wifi_v_enter_normal_mode( void ){

    GPIO_InitTypeDef GPIO_InitStruct;

    // set up IO
    GPIO_InitStruct.Pin         = WIFI_BOOT_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_IT_FALLING; // falling edge triggered
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLUP; // set boot pin to high
    HAL_GPIO_Init(WIFI_BOOT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_PD_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_PD_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_RESET); // hold chip in reset

    _delay_ms(WIFI_RESET_DELAY_MS);

    GPIO_InitStruct.Pin         = WIFI_CTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_CTS_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_CTS_GPIO_Port, WIFI_CTS_Pin, GPIO_PIN_RESET); // pull to ground while we boot up

    GPIO_InitStruct.Pin         = WIFI_RTS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RTS_Port, &GPIO_InitStruct);

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_SET);

    _delay_ms(WIFI_RESET_DELAY_MS);

    // NOTE! leave CTS pulled down, device is not finished booting up.
    // CTS should be active HIGH, not low, to handle this.
    // HAL_GPIO_WritePin(WIFI_CTS_GPIO_Port, WIFI_CTS_Pin, GPIO_PIN_SET);

    // re-init uart
    GPIO_InitStruct.Pin = WIFI_RXD_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    #ifdef BOARD_CHROMATRONX
    GPIO_InitStruct.Alternate = GPIO_AF8_UART8;
    #else
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    #endif
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(WIFI_RXD_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = WIFI_TXD_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    #ifdef BOARD_CHROMATRONX
    GPIO_InitStruct.Alternate = GPIO_AF8_UART8;
    #else
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    #endif
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(WIFI_TXD_GPIO_Port, &GPIO_InitStruct);

    wifi_usart.Init.BaudRate = 4000000;
    wifi_usart.Init.WordLength = UART_WORDLENGTH_8B;
    wifi_usart.Init.StopBits = UART_STOPBITS_1;
    wifi_usart.Init.Parity = UART_PARITY_NONE;
    wifi_usart.Init.Mode = UART_MODE_TX_RX;
    wifi_usart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    wifi_usart.Init.OverSampling = UART_OVERSAMPLING_16;
    wifi_usart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    #ifdef BOARD_CHROMATRONX
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    #else
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    #endif
    wifi_usart.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
    if (HAL_UART_Init(&wifi_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    normal_mode = TRUE;
}


