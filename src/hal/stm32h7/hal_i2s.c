/*
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
 */

#include "system.h"
#include "threading.h"

#include "hal_io.h"
#include "hal_i2s.h"

#ifndef BOARD_CHROMATRONX

typedef struct{
    uint32_t pin;
    uint32_t alt;
} hal_i2s_ch_t;

static const hal_i2s_ch_t i2s_io[] = {
    { IO_PIN_GPIOA2, GPIO_AF5_SPI1 }, // SDI
    { IO_PIN_GPIOA3, GPIO_AF5_SPI1 }, // CK
    { IO_PIN_GPIOA4, GPIO_AF5_SPI1 }, // WS
};


static uint8_t sample_bits;
static uint8_t sample_bit_shift;

#define I2S_DMA_BUF_SIZE ( I2S_BUF_SIZE * 2 )

static NON_CACHEABLE int32_t i2s_dma_buffer[I2S_DMA_BUF_SIZE];
static volatile bool i2s_buffer_ready;
static volatile bool i2s_buffer_overrun;


static int32_t i2s_buffer[I2S_BUF_SIZE];

static int32_t left_buffer[I2S_BUF_SIZE / 2];
static int32_t right_buffer[I2S_BUF_SIZE / 2];

static I2S_HandleTypeDef i2s_handle;
static DMA_HandleTypeDef i2s_dma;



ISR(I2S_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &i2s_dma );
}

ISR(I2S_HANDLER){

    HAL_I2S_IRQHandler( &i2s_handle );
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){

    if( i2s_buffer_ready ){

        // overrun
        i2s_buffer_overrun = TRUE;
        return;
    }

    i2s_buffer_ready = TRUE;

    memcpy( i2s_buffer, &i2s_dma_buffer[I2S_BUF_SIZE], sizeof(i2s_buffer) );
    thread_v_signal( I2S_SIGNAL );
}

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s){

    if( i2s_buffer_ready ){

        // overrun
        i2s_buffer_overrun = TRUE;
        return;
    }

    i2s_buffer_ready = TRUE;

    memcpy( i2s_buffer, i2s_dma_buffer, sizeof(i2s_buffer) );
    thread_v_signal( I2S_SIGNAL );
}


PT_THREAD( i2s_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_SIGNAL( pt, I2S_SIGNAL );
        
        int32_t *ptr = i2s_buffer;

        for( uint32_t i = 0; i < I2S_BUF_SIZE / 2; i++ ){

            right_buffer[i] = ( *ptr++ & 0xffffc0 ) << 8;
            left_buffer[i] = ( *ptr++ & 0xffffc0 ) << 8;
        }

        if( i2s_v_callback ){

            i2s_v_callback( left_buffer, right_buffer, I2S_BUF_SIZE / 2 );
        }

        // clear buffer
        ATOMIC;
        i2s_buffer_ready = FALSE;
        END_ATOMIC;
    }   
    
    
PT_END( pt );
}


void hal_i2s_v_init( void ){

	i2s_handle.Instance = I2S;

	GPIO_TypeDef *port;
	GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    hal_io_v_get_port( i2s_io[0].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = i2s_io[0].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );

    hal_io_v_get_port( i2s_io[1].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = i2s_io[1].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );

    hal_io_v_get_port( i2s_io[2].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = i2s_io[2].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );
}

void hal_i2s_v_start( uint16_t sample_rate, uint8_t _sample_bits ){

    sample_bits = _sample_bits;
    sample_bit_shift = 24 - sample_bits;

	__HAL_RCC_SPI1_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();

	i2s_handle.Init.Mode 				= I2S_MODE_MASTER_RX;
	i2s_handle.Init.Standard 			= I2S_STANDARD_PHILIPS;
	i2s_handle.Init.DataFormat 			= I2S_DATAFORMAT_24B;
	i2s_handle.Init.MCLKOutput 			= I2S_MCLKOUTPUT_DISABLE;
	i2s_handle.Init.AudioFreq 			= sample_rate;
	i2s_handle.Init.CPOL 				= I2S_CPOL_HIGH; // clock in on falling edge
	i2s_handle.Init.FirstBit 			= I2S_FIRSTBIT_MSB;
	i2s_handle.Init.WSInversion 		= I2S_WS_INVERSION_DISABLE;
	i2s_handle.Init.IOSwap 				= I2S_IO_SWAP_DISABLE;
	i2s_handle.Init.Data24BitAlignment 	= I2S_DATA_24BIT_ALIGNMENT_LEFT;
	i2s_handle.Init.FifoThreshold 		= I2S_FIFO_THRESHOLD_01DATA;
	i2s_handle.Init.MasterKeepIOState 	= I2S_MASTER_KEEP_IO_STATE_DISABLE;
	i2s_handle.Init.SlaveExtendFREDetection = I2S_SLAVE_EXTEND_FRE_DETECTION_DISABLE;

	HAL_I2S_Init( &i2s_handle );

	i2s_dma.Instance                  = I2S_DMA_INSTANCE;
    i2s_dma.Init.Request              = I2S_DMA_REQUEST;
    i2s_dma.Init.Direction            = DMA_PERIPH_TO_MEMORY;
    i2s_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    i2s_dma.Init.MemInc               = DMA_MINC_ENABLE;
    i2s_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_WORD;
    i2s_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_WORD;
    i2s_dma.Init.Mode                 = DMA_CIRCULAR;
    i2s_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    i2s_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    i2s_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    i2s_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    i2s_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_DMA_Init( &i2s_dma );

    HAL_NVIC_SetPriority( I2S_DMA_IRQ, 0, 0 );
    HAL_NVIC_EnableIRQ( I2S_DMA_IRQ );

    HAL_NVIC_SetPriority( I2S_SPI_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( I2S_SPI_IRQn );

    __HAL_LINKDMA( &i2s_handle, hdmarx, i2s_dma );

    HAL_I2S_Receive_DMA( &i2s_handle, (uint16_t *)i2s_dma_buffer, I2S_DMA_BUF_SIZE / 2 );
    
    if((i2s_handle.Instance->I2SCFGR & SPI_I2SCFGR_I2SCFG) == I2S_MODE_MASTER_RX)
    {
      /* Clear the Overrun Flag */ 
      __HAL_I2S_CLEAR_OVRFLAG(&i2s_handle);
    }

    __HAL_I2S_ENABLE(&i2s_handle);

    if(IS_I2S_MASTER(i2s_handle.Init.Mode))
    {
      	i2s_handle.Instance->CR1 |= SPI_CR1_CSTART;
    }

    thread_t_create( i2s_thread,
                 PSTR("i2s"),
                 0,
                 0 );
}

#endif
