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


#include "system.h"
#include "timers.h"

#include "hal_usart.h"
#include "hal_io.h"

#include "hal_esp8266.h"
#include "wifi_cmd.h"

#include "esp8266.h"


#define WIFI_RESET_DELAY_MS     20


static NON_CACHEABLE uint8_t rx_dma_buffer[WIFI_UART_RX_BUF_SIZE + 4]; // pad to multiple of DMA transfer block size (7 bytes)
// static uint8_t rx_dma_buffer[WIFI_UART_RX_BUF_SIZE + 4]; // pad to multiple of DMA transfer block size (7 bytes)
// the padding is in case the wifi sends more data than it is supposed to.

static volatile bool buffer_busy;
static uint8_t rx_buf[WIFI_UART_RX_BUF_SIZE];

static uint32_t current_tx_bytes;
static uint32_t current_rx_bytes;

static volatile bool wifi_rx_ready;

static uint32_t ready_time_start;
static volatile uint16_t max_ready_wait_isr;

static USART_t wifi_usart;
static DMA_HandleTypeDef wifi_dma;
static TIM_HandleTypeDef wifi_timer;


volatile uint32_t meow;

void DMA2_Stream2_IRQHandler( void ){
        
    HAL_DMA_IRQHandler( &wifi_dma );

// // //     // disable IRQ
    // DMA.WIFI_DMA_CH.CTRLB = 0;
//     __HAL_DMA_DISABLE( &wifi_dma );

    if( rx_dma_buffer[0] == WIFI_COMM_DATA ){

//         // we can't set a TRFCNT interrupt to inform us when the message is finished,
//         // because at this point we are already somewhere in the middle of receiving it
//         // and we don't want to mess up the DMA transfer.
//         // however, at this point, we do know how long the message is, and therefore, how
//         // long it will take to receive it.  we can set a timer to fire when the message
//         // is finished.

//         // reset timer
//         TCD1.CTRLA = 0;
//         TCD1.CTRLB = 0;
//         TCD1.CNT = 0;
        
        wifi_data_header_t *header = (wifi_data_header_t *)&rx_dma_buffer[1];
        meow = header->len;

//         // calculate timer length based on packet length
//         // at 4 MHz USART, each byte is 2.5 microseconds.
//         // we tick at 4 MHz, which yields 10 ticks per byte.
//         TCD1.PER = header->len * 10;

//         // start timer
//         TCD1.CTRLA = TC_CLKSEL_DIV8_gc;
    }
    else{

        // incorrect control byte on frame

        // reset DMA and send ready signal
        hal_wifi_v_set_rx_ready();
    }
}

void WIFI_TIMER_ISR( void ){

    // disable timer and make sure interrupt flags are cleared.
    // this is important because on very short messages, the timer
    // may set a second pending OVF interrupt before the timer is
    // disabled while handling the first interrupt.  this will
    // cause the interrupt to run again as if there were a second message
    // being processed.  this will then cause the buffer wait logic to trigger,
    // causing the timer to fire again in 100 uS.  once the original message
    // has been processed and the buffer freed, the interrupt handler will
    // then think that it can copy the second (non-existant) message into the
    // main buffer and reset the receive ready flag and the DMA engine.
    // if this occurs while a message was being received, it breaks the interface.
    // TCD1.CTRLA = 0;
    // TCD1.INTFLAGS = TC0_OVFIF_bm;

    if( buffer_busy ){

        // check again in 100 microseconds
        // TCD1.PER = 400;
        // TCD1.CTRLA = TC_CLKSEL_DIV8_gc;
        
        return;
    }

    buffer_busy = TRUE;

    // copy to process buffer
    memcpy( rx_buf, rx_dma_buffer, sizeof(rx_buf) );

    hal_wifi_v_set_rx_ready();

    // packet complete
    thread_v_signal( WIFI_SIGNAL );
}

void USART6_IRQHandler( void )
{ 
    HAL_UART_IRQHandler( &wifi_usart );
}



// GPIO RX ready IRQ
void EXTI9_5_IRQHandler( void ){
// OS_IRQ_BEGIN(WIFI_IRQ_VECTOR);

    if( __HAL_GPIO_EXTI_GET_FLAG( GPIO_PIN_8 ) ){

        __HAL_GPIO_EXTI_CLEAR_FLAG( GPIO_PIN_8 );        

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

    wifi_usart.Instance = WIFI_USART;

    // set up DMA
    wifi_dma.Instance                  = WIFI_DMA;
    wifi_dma.Init.Channel              = WIFI_DMA_CHANNEL;
    wifi_dma.Init.Direction            = DMA_PERIPH_TO_MEMORY;
    wifi_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    wifi_dma.Init.MemInc               = DMA_MINC_ENABLE;
    wifi_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    wifi_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    wifi_dma.Init.Mode                 = DMA_NORMAL;
    wifi_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    wifi_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    // wifi_dma.Init.FIFOMode             = DMA_FIFOMODE_ENABLE;
    wifi_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_HALFFULL; 
    wifi_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    wifi_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;
    // wifi_dma.Init.MemBurst             = DMA_MBURST_INC4;
    // wifi_dma.Init.PeriphBurst          = DMA_PBURST_INC4;

    // enable clocks
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
        
    // reset timer
    // Timer 3 is on APB1 (108 MHz nominally)
    wifi_timer.Instance = WIFI_TIMER;

    wifi_timer.Init.Prescaler          = 108;
    wifi_timer.Init.Period             = 65535;
    wifi_timer.Init.CounterMode        = TIM_COUNTERMODE_UP;
    wifi_timer.Init.AutoReloadPreload  = TIM_AUTORELOAD_PRELOAD_ENABLE;
    wifi_timer.Init.ClockDivision      = TIM_CLOCKDIVISION_DIV1;
    wifi_timer.Init.RepetitionCounter  = 0;

    HAL_TIM_Base_Init( &wifi_timer );

    HAL_NVIC_SetPriority( TIM3_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( TIM3_IRQn );



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

    // USART_XCK PIN MISSING
    GPIO_InitStruct.Pin         = WIFI_RX_Ready_Pin;
    GPIO_InitStruct.Mode        = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_PULLDOWN;
    HAL_GPIO_Init(WIFI_RX_Ready_Port, &GPIO_InitStruct);

    // hold module in reset
    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_RESET);
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

	usart_v_send_byte( &wifi_usart, b );

    current_tx_bytes += 1;
}

void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len ){

	usart_v_send_data( &wifi_usart, data, len );

    current_tx_bytes += len;
}

int16_t hal_wifi_i16_usart_get_char( void ){

	return usart_i16_get_byte( &wifi_usart );
}

void hal_wifi_v_usart_flush( void ){

	BUSY_WAIT( hal_wifi_i16_usart_get_char() >= 0 );
}

uint16_t hal_wifi_u16_dma_rx_bytes( void ){

    uint16_t len = 0;

    // uint16_t dest_addr0, dest_addr1;

    ATOMIC;

    // do{

    //     volatile uint8_t temp;
    //     temp = DMA.WIFI_DMA_CH.DESTADDR0;
    //     dest_addr0 = temp + ( (uint16_t)DMA.WIFI_DMA_CH.DESTADDR1 << 8 );

    //     temp = DMA.WIFI_DMA_CH.DESTADDR0;
    //     dest_addr1 = temp + ( (uint16_t)DMA.WIFI_DMA_CH.DESTADDR1 << 8 );

    // } while( dest_addr0 != dest_addr1 );

    // len = dest_addr0 - (uint16_t)rx_dma_buffer;
    len = sizeof(rx_dma_buffer) - __HAL_DMA_GET_COUNTER( &wifi_dma );

    END_ATOMIC;

    return len;
}

void hal_wifi_v_disable_rx_dma( void ){

	ATOMIC;

    HAL_UART_DMAStop( &wifi_usart );

    HAL_DMA_DeInit( &wifi_dma );

 //    DMA.WIFI_DMA_CH.CTRLA &= ~DMA_CH_ENABLE_bm;
 //    DMA.WIFI_DMA_CH.TRFCNT = 0;
    HAL_NVIC_DisableIRQ( DMA1_Stream0_IRQn );


 //    // make sure DMA timer is disabled
 //    TCD1.CTRLA = 0;
 //    TCD1.INTFLAGS = TC0_OVFIF_bm;
    END_ATOMIC;
}	

void hal_wifi_v_enable_rx_dma( bool irq ){

	ATOMIC;

    hal_wifi_v_disable_rx_dma();

    // flush buffer
    hal_wifi_v_usart_flush();

    HAL_DMA_Init( &wifi_dma );

    // __HAL_DMA_ENABLE( &wifi_dma );

    __HAL_LINKDMA( &wifi_usart, hdmarx, wifi_dma );

 //    DMA.INTFLAGS = WIFI_DMA_CHTRNIF | WIFI_DMA_CHERRIF; // clear transaction complete interrupt

 //    DMA.WIFI_DMA_CH.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_REPEAT_bm | DMA_CH_BURSTLEN_1BYTE_gc;
 //    DMA.WIFI_DMA_CH.CTRLB = 0;
 //    DMA.WIFI_DMA_CH.REPCNT = 0;
 //    DMA.WIFI_DMA_CH.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_INC_gc;
 //    DMA.WIFI_DMA_CH.TRIGSRC = WIFI_USART_DMA_TRIG;
 //    DMA.WIFI_DMA_CH.TRFCNT = sizeof(wifi_data_header_t) + 1;

 //    DMA.WIFI_DMA_CH.SRCADDR0 = ( ( (uint16_t)&WIFI_USART.DATA ) >> 0 ) & 0xFF;
 //    DMA.WIFI_DMA_CH.SRCADDR1 = ( ( (uint16_t)&WIFI_USART.DATA ) >> 8 ) & 0xFF;
 //    DMA.WIFI_DMA_CH.SRCADDR2 = 0;

 //    DMA.WIFI_DMA_CH.DESTADDR0 = ( ( (uint16_t)rx_dma_buffer ) >> 0 ) & 0xFF;
 //    DMA.WIFI_DMA_CH.DESTADDR1 = ( ( (uint16_t)rx_dma_buffer ) >> 8 ) & 0xFF;
 //    DMA.WIFI_DMA_CH.DESTADDR2 = 0;

    if( irq ){

        HAL_NVIC_SetPriority( DMA2_Stream2_IRQn, 0, 0 );
        HAL_NVIC_EnableIRQ( DMA2_Stream2_IRQn );

        __HAL_DMA_ENABLE_IT( &wifi_dma, DMA_IT_TC );

 //        DMA.WIFI_DMA_CH.CTRLB = DMA_CH_TRNINTLVL_HI_gc; // enable transfer complete interrupt
        // HAL_DMA_Start_IT( &wifi_dma, (uint32_t)&wifi_usart.Instance->RDR, (uint32_t)rx_dma_buffer, sizeof(wifi_data_header_t) + 1 );
    }
    else{

        // HAL_DMA_Start( &wifi_dma, (uint32_t)&wifi_usart.Instance->RDR, (uint32_t)rx_dma_buffer, sizeof(wifi_data_header_t) + 1 );
    }

    // hal_cpu_v_clean_d_cache();
    // hal_cpu_v_clean_d_cache_by_addr( (uint32_t *)rx_dma_buffer, sizeof(rx_dma_buffer) );
    // hal_cpu_v_clean_and_invalidate_d_cache();

    __HAL_UART_CLEAR_IT( &wifi_usart, UART_FLAG_ORE );        

    // HAL_NVIC_SetPriority( USART6_IRQn, 0, 0 );                                        
    // HAL_NVIC_EnableIRQ( USART6_IRQn );

    // HAL_NVIC_SetPriority( DMA2_Stream2_IRQn, 0, 0 );
    // HAL_NVIC_EnableIRQ( DMA2_Stream2_IRQn );

    // HAL_UART_Receive_DMA( &wifi_usart, rx_dma_buffer, sizeof(wifi_data_header_t) + 1 );
    HAL_UART_Receive_DMA( &wifi_usart, rx_dma_buffer, sizeof(rx_dma_buffer) );

 //    DMA.WIFI_DMA_CH.CTRLA |= DMA_CH_ENABLE_bm;

    END_ATOMIC;
}

void hal_wifi_v_usart_set_baud( baud_t baud ){

    usart_v_set_baud( &wifi_usart, baud );    
}

void hal_wifi_v_reset_rx_buffer( void ){

	hal_wifi_v_clear_rx_buffer();

    // set up DMA
    hal_wifi_v_enable_rx_dma( TRUE );
}

void hal_wifi_v_clear_rx_buffer( void ){

	memset( rx_dma_buffer, 0xff, sizeof(rx_dma_buffer) );
}

void hal_wifi_v_release_rx_buffer( void ){

	current_rx_bytes += hal_wifi_i16_rx_data_received();

    ATOMIC;

    // release buffer
    buffer_busy = FALSE;

    END_ATOMIC;
}

void hal_wifi_v_reset_control_byte( void ){

    rx_buf[0] = WIFI_COMM_IDLE;   
}

void hal_wifi_v_reset_comm( void ){

	hal_wifi_v_reset_rx_buffer();
    hal_wifi_v_usart_send_char( WIFI_COMM_RESET );   
}

void hal_wifi_v_set_rx_ready( void ){

	hal_wifi_v_reset_rx_buffer();

    HAL_GPIO_WritePin(WIFI_RX_Ready_Port, WIFI_RX_Ready_Pin, GPIO_PIN_RESET);
    _delay_us( 20 );
    HAL_GPIO_WritePin(WIFI_RX_Ready_Port, WIFI_RX_Ready_Pin, GPIO_PIN_SET);
}

void hal_wifi_v_disable_irq( void ){

    HAL_NVIC_DisableIRQ( EXTI9_5_IRQn );
}

void hal_wifi_v_enable_irq( void ){

    HAL_NVIC_EnableIRQ( EXTI9_5_IRQn );
}


uint8_t hal_wifi_u8_get_control_byte( void ){

	return rx_buf[0];
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


// pin connection missing
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

    HAL_GPIO_WritePin(WIFI_PD_GPIO_Port, WIFI_PD_Pin, GPIO_PIN_RESET);

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
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(WIFI_RXD_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = WIFI_TXD_Pin;
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(WIFI_TXD_GPIO_Port, &GPIO_InitStruct);

    wifi_usart.Init.BaudRate = 115200;
    wifi_usart.Init.WordLength = UART_WORDLENGTH_8B;
    wifi_usart.Init.StopBits = UART_STOPBITS_1;
    wifi_usart.Init.Parity = UART_PARITY_NONE;
    wifi_usart.Init.Mode = UART_MODE_TX_RX;
    wifi_usart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    wifi_usart.Init.OverSampling = UART_OVERSAMPLING_16;
    wifi_usart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    // wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    wifi_usart.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
    if (HAL_UART_Init(&wifi_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    hal_wifi_v_disable_rx_dma();

    hal_wifi_v_usart_flush();

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    // WIFI_PD_PORT.OUTSET = ( 1 << WIFI_PD_PIN );
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
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(WIFI_RXD_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = WIFI_TXD_Pin;
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(WIFI_TXD_GPIO_Port, &GPIO_InitStruct);

    wifi_usart.Init.BaudRate = 4000000;
    wifi_usart.Init.WordLength = UART_WORDLENGTH_8B;
    wifi_usart.Init.StopBits = UART_STOPBITS_1;
    wifi_usart.Init.Parity = UART_PARITY_NONE;
    wifi_usart.Init.Mode = UART_MODE_TX_RX;
    wifi_usart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    wifi_usart.Init.OverSampling = UART_OVERSAMPLING_16;
    wifi_usart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    // wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    wifi_usart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    wifi_usart.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
    if (HAL_UART_Init(&wifi_usart) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }


    // connect BOOT pin to EXTI IRQ 8
    HAL_NVIC_SetPriority( EXTI9_5_IRQn, 0, 0 );
    HAL_NVIC_DisableIRQ( EXTI9_5_IRQn );



}


