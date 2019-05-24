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

// static const uint8_t wifi_firmware[] = {
//     #include "wifi_firmware.txt"
// };

#define WIFI_RESET_DELAY_MS     20


static NON_CACHEABLE uint8_t rx_dma_buffer[WIFI_UART_RX_BUF_SIZE];

static volatile bool buffer_busy;
static uint8_t rx_buf[WIFI_UART_RX_BUF_SIZE];

static uint32_t current_tx_bytes;
static uint32_t current_rx_bytes;

static volatile bool wifi_rx_ready;

static uint32_t ready_time_start;
static volatile uint16_t max_ready_wait_isr;

static USART_t wifi_usart;
static DMA_HandleTypeDef wifi_dma;


static bool normal_mode;
static uint8_t control_byte;

static uint16_t timeouts;

KV_SECTION_META kv_meta_t hal_wifi_kv[] = {
    { SAPPHIRE_TYPE_UINT16,      0, KV_FLAGS_READ_ONLY, &timeouts, 0,   "wifi_hal_timeouts" },
};

PT_THREAD( hal_wifi_thread( pt_t *pt, void *state ) );


static volatile uint16_t get_dma_bytes( void ){

    ATOMIC;
    uint16_t temp = sizeof(rx_dma_buffer) - __HAL_DMA_GET_COUNTER( &wifi_dma );
    END_ATOMIC;

    return temp;
}

// GPIO RX ready IRQ
#ifdef BOARD_CHROMATRONX
ISR(EXTI3_IRQHandler){
#else
ISR(EXTI2_IRQHandler){
#endif
// OS_IRQ_BEGIN(WIFI_IRQ_VECTOR);

    if( __HAL_GPIO_EXTI_GET_FLAG( WIFI_BOOT_Pin ) ){
        __HAL_GPIO_EXTI_CLEAR_FLAG( WIFI_BOOT_Pin );        

        wifi_rx_ready = TRUE;

        uint32_t elapsed_ready_time = tmr_u32_elapsed_time_us( ready_time_start );

        if( elapsed_ready_time > 60000 ){

            return;
        }

        if( elapsed_ready_time > max_ready_wait_isr ){

            max_ready_wait_isr = elapsed_ready_time;
        }
    }

// OS_IRQ_END();
}



void hal_wifi_v_init( void ){

    // enable clocks
    #ifdef BOARD_CHROMATRONX
    __HAL_RCC_UART8_CLK_ENABLE();
    #else
    __HAL_RCC_UART4_CLK_ENABLE();
    #endif
    
    __HAL_RCC_DMA1_CLK_ENABLE();
    

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

    // set up DMA
    wifi_dma.Instance                  = WIFI_DMA;
    wifi_dma.Init.Request              = WIFI_DMA_REQUEST;
    wifi_dma.Init.Direction            = DMA_PERIPH_TO_MEMORY;
    wifi_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    wifi_dma.Init.MemInc               = DMA_MINC_ENABLE;
    wifi_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    wifi_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    wifi_dma.Init.Mode                 = DMA_NORMAL;
    wifi_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    wifi_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    wifi_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    wifi_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    wifi_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_NVIC_SetPriority( WIFI_DMA_IRQ, 0, 0 );
    HAL_NVIC_DisableIRQ( WIFI_DMA_IRQ );
        
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

    GPIO_InitStruct.Pin         = WIFI_SS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_SS_GPIO_Port, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin         = WIFI_BOOT_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_BOOT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_RX_Ready_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RX_Ready_Port, &GPIO_InitStruct);

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

    static wifi_data_header_t *header;


    // copy firmware to file
    // ffs_fw_i32_write( 2, 0, wifi_firmware, sizeof(wifi_firmware) );

    rx_dma_buffer[0] = WIFI_COMM_IDLE;

    while(1){

        THREAD_YIELD( pt );

        THREAD_WAIT_WHILE( pt, !normal_mode );

        THREAD_WAIT_WHILE( pt, rx_dma_buffer[0] == WIFI_COMM_IDLE );

        if( rx_dma_buffer[0] == WIFI_COMM_QUERY_READY ){

            // set control byte for main ESP thread
            control_byte = rx_dma_buffer[0];

            rx_dma_buffer[0] = WIFI_COMM_IDLE;
        }
        else if( rx_dma_buffer[0] == WIFI_COMM_DATA ){

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_THREAD_TIMEOUT );
            THREAD_WAIT_WHILE( pt, buffer_busy && thread_b_alarm_set() );

            // check for timeout
            if( thread_b_alarm() ){

                trace_printf("timeout1\r\n");

                timeouts++;
                THREAD_RESTART( pt );
            }

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_THREAD_TIMEOUT );
            THREAD_WAIT_WHILE( pt, ( get_dma_bytes() < ( sizeof(wifi_data_header_t) + 1 ) ) &&
                                   ( thread_b_alarm_set() ) );

            // check for timeout
            if( thread_b_alarm() ){

                trace_printf("timeout2\r\n");

                timeouts++;
                THREAD_RESTART( pt );
            }

            header = (wifi_data_header_t *)&rx_dma_buffer[1];

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_THREAD_TIMEOUT );
            THREAD_WAIT_WHILE( pt, ( get_dma_bytes() < ( sizeof(wifi_data_header_t) + 1 + header->len ) ) &&
                                   ( thread_b_alarm_set() ) );            
            
            // check for timeout
            if( thread_b_alarm() ){

                trace_printf("timeout3\r\n");

                timeouts++;
                THREAD_RESTART( pt );
            }

            current_rx_bytes += sizeof(wifi_data_header_t) + 1 + header->len;

            // copy to process buffer
            memcpy( rx_buf, rx_dma_buffer, sizeof(rx_buf) );

            // clear control byte
            rx_dma_buffer[0] = WIFI_COMM_IDLE;

            // signal ESP our receive buffer is ready
            hal_wifi_v_set_rx_ready();

            buffer_busy = TRUE;;

            // set control byte for wifi process
            control_byte = WIFI_COMM_DATA;

            // signal thread
            thread_v_signal( WIFI_SIGNAL );
        }   
    }

PT_END( pt );
}


void hal_wifi_v_reset( void ){

}

uint8_t *hal_wifi_u8p_get_rx_dma_buf_ptr( void ){

	return rx_dma_buffer;
}

uint8_t *hal_wifi_u8p_get_rx_buf_ptr( void ){

	return rx_buf;
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

void hal_wifi_v_usart_flush( void ){

	BUSY_WAIT( hal_wifi_i16_usart_get_char() >= 0 );
}

uint16_t hal_wifi_u16_dma_rx_bytes( void ){

    return get_dma_bytes();
}

void hal_wifi_v_disable_rx_dma( void ){

	ATOMIC;

    HAL_UART_DMAStop( &wifi_usart );

    HAL_DMA_DeInit( &wifi_dma );

    END_ATOMIC;
}	

void hal_wifi_v_enable_rx_dma( bool irq ){
    
	ATOMIC;

    hal_wifi_v_disable_rx_dma();

    // flush buffer
    hal_wifi_v_usart_flush();

    HAL_DMA_Init( &wifi_dma );

    __HAL_LINKDMA( &wifi_usart, hdmarx, wifi_dma );

    __HAL_UART_CLEAR_OREFLAG( &wifi_usart );   
    
    HAL_UART_Receive_DMA( &wifi_usart, rx_dma_buffer, sizeof(rx_dma_buffer) );

    END_ATOMIC;
}

void hal_wifi_v_usart_set_baud( baud_t baud ){

    wifi_usart.Init.BaudRate = baud;
    
    if (HAL_UART_Init(&wifi_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }
}

void hal_wifi_v_reset_rx_buffer( void ){

    // set up DMA
    hal_wifi_v_enable_rx_dma( TRUE );
}

void hal_wifi_v_clear_rx_buffer( void ){

	memset( rx_dma_buffer, 0xff, sizeof(rx_dma_buffer) );
}

void hal_wifi_v_release_rx_buffer( void ){

    ATOMIC;

    // release buffer
    buffer_busy = FALSE;

    END_ATOMIC;
}

void hal_wifi_v_reset_control_byte( void ){

    control_byte = WIFI_COMM_IDLE;
}

void hal_wifi_v_reset_comm( void ){

    hal_wifi_v_usart_send_char( WIFI_COMM_RESET );   
}

void hal_wifi_v_set_rx_ready( void ){

	hal_wifi_v_reset_rx_buffer();

    HAL_GPIO_WritePin(WIFI_RX_Ready_Port, WIFI_RX_Ready_Pin, GPIO_PIN_RESET);
    _delay_us( 20 );
    HAL_GPIO_WritePin(WIFI_RX_Ready_Port, WIFI_RX_Ready_Pin, GPIO_PIN_SET);
}

void hal_wifi_v_disable_irq( void ){

    #ifdef BOARD_CHROMATRONX
    HAL_NVIC_DisableIRQ( EXTI3_IRQn );
    #else
    HAL_NVIC_DisableIRQ( EXTI2_IRQn );
    #endif
}

void hal_wifi_v_enable_irq( void ){

    #ifdef BOARD_CHROMATRONX
    HAL_NVIC_EnableIRQ( EXTI3_IRQn );
    #else
    HAL_NVIC_EnableIRQ( EXTI2_IRQn );
    #endif
}


uint8_t hal_wifi_u8_get_control_byte( void ){

    return control_byte;
}	

int16_t hal_wifi_i16_rx_data_received( void ){

    if( hal_wifi_u8_get_control_byte() == WIFI_COMM_IDLE ){

        return -1;
    }

    wifi_data_header_t *header = (wifi_data_header_t *)&rx_buf[1];

    return ( sizeof(wifi_data_header_t) + header->len + 1 );
}	

void hal_wifi_v_clear_rx_ready( void ){

	ATOMIC;
    wifi_rx_ready = FALSE;

    ready_time_start = tmr_u32_get_system_time_us();

    END_ATOMIC;
}

bool hal_wifi_b_comm_ready( void ){

	ATOMIC;
    bool temp = wifi_rx_ready;
    END_ATOMIC;

    return temp;
}	

uint32_t hal_wifi_u32_get_max_ready_wait( void ){

	ATOMIC;

	uint32_t temp = max_ready_wait_isr;

	END_ATOMIC;

	return temp;
}

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


    GPIO_InitStruct.Pin         = WIFI_RX_Ready_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RX_Ready_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = WIFI_BOOT_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_BOOT_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin         = WIFI_SS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_SS_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_SS_GPIO_Port, WIFI_SS_Pin, GPIO_PIN_RESET);
    
    hal_wifi_v_disable_irq();

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

    hal_wifi_v_disable_rx_dma();

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

    GPIO_InitStruct.Pin         = WIFI_SS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_SS_GPIO_Port, &GPIO_InitStruct);
}

void hal_wifi_v_enter_normal_mode( void ){

    GPIO_InitTypeDef GPIO_InitStruct;

    hal_wifi_v_disable_irq();

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

    GPIO_InitStruct.Pin         = WIFI_SS_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_SS_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_SS_GPIO_Port, WIFI_SS_Pin, GPIO_PIN_RESET);

    // set RX ready pin to output
    GPIO_InitStruct.Pin         = WIFI_RX_Ready_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RX_Ready_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WIFI_RX_Ready_Port, WIFI_RX_Ready_Pin, GPIO_PIN_SET);

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_SET);

    _delay_ms(WIFI_RESET_DELAY_MS);

    hal_wifi_v_disable_rx_dma();

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

    #ifdef BOARD_CHROMATRONX
    // connect BOOT pin to EXTI IRQ 3
    HAL_NVIC_SetPriority( EXTI3_IRQn, 0, 0 );
    HAL_NVIC_DisableIRQ( EXTI3_IRQn );

    #else

    HAL_NVIC_SetPriority( EXTI2_IRQn, 0, 0 );
    HAL_NVIC_DisableIRQ( EXTI2_IRQn );

    #endif


    normal_mode = TRUE;
}


