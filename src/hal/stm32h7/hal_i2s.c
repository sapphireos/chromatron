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

#include "hal_io.h"
#include "hal_i2s.h"


typedef struct{
	SPI_TypeDef *spi;
	uint32_t pin0;
	GPIO_TypeDef *port0;
	uint8_t alt0;
	uint32_t pin1;
	GPIO_TypeDef *port1;
	uint8_t alt1;
	uint32_t pin2;
	GPIO_TypeDef *port2;
	uint8_t alt2;
} i2s_port_def_t;

static const i2s_port_def_t i2s_port_def = {
	I2S, 
	SPI1_CS_Pin, 
	SPI1_CS_GPIO_Port, 
	GPIO_AF5_SPI1,
	SPI1_MISO_Pin, 
	SPI1_MISO_GPIO_Port, 
	GPIO_AF5_SPI1,
	PIX_CLK_0_Pin, 
	PIX_CLK_0_GPIO_Port, 
	GPIO_AF5_SPI1,
};

static uint16_t i2s_buffer[1024];

static I2S_HandleTypeDef i2s_handle;
static DMA_HandleTypeDef i2s_dma;

ISR(I2S_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &i2s_dma );
}

ISR(I2S_HANDLER){

    HAL_I2S_IRQHandler( &i2s_handle );
}

void hal_i2s_v_init( void ){

	i2s_handle.Instance = I2S;

	GPIO_InitTypeDef GPIO_InitStruct;
	
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    // GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;	

    // init IO
	GPIO_InitStruct.Pin 		= i2s_port_def.pin0;
	GPIO_InitStruct.Alternate 	= i2s_port_def.alt0;
	HAL_GPIO_Init( i2s_port_def.port0, &GPIO_InitStruct );
	
	GPIO_InitStruct.Pin 		= i2s_port_def.pin1;
	GPIO_InitStruct.Alternate 	= i2s_port_def.alt1;
	HAL_GPIO_Init( i2s_port_def.port1, &GPIO_InitStruct );

	GPIO_InitStruct.Pin 		= i2s_port_def.pin2;
	GPIO_InitStruct.Alternate 	= i2s_port_def.alt2;
	HAL_GPIO_Init( i2s_port_def.port2, &GPIO_InitStruct );
	// HAL_GPIO_WritePin(i2s_port_def.port2, i2s_port_def.pin2, GPIO_PIN_SET);


}

void hal_i2s_v_start( uint16_t sample_rate, uint8_t sample_bits, bool stereo ){

	__HAL_RCC_SPI1_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();

	i2s_handle.Init.Mode 				= I2S_MODE_MASTER_RX;
	i2s_handle.Init.Standard 			= I2S_STANDARD_MSB;
	i2s_handle.Init.DataFormat 			= I2S_DATAFORMAT_24B;
	i2s_handle.Init.MCLKOutput 			= I2S_MCLKOUTPUT_ENABLE;
	i2s_handle.Init.AudioFreq 			= I2S_AUDIOFREQ_22K;
	i2s_handle.Init.CPOL 				= I2S_CPOL_LOW;
	i2s_handle.Init.FirstBit 			= I2S_FIRSTBIT_MSB;
	i2s_handle.Init.WSInversion 		= I2S_WS_INVERSION_DISABLE;
	i2s_handle.Init.IOSwap 				= I2S_IO_SWAP_DISABLE;
	i2s_handle.Init.Data24BitAlignment 	= I2S_DATA_24BIT_ALIGNMENT_RIGHT;
	i2s_handle.Init.FifoThreshold 		= I2S_FIFO_THRESHOLD_01DATA;
	i2s_handle.Init.MasterKeepIOState 	= I2S_MASTER_KEEP_IO_STATE_DISABLE;
	i2s_handle.Init.SlaveExtendFREDetection = I2S_SLAVE_EXTEND_FRE_DETECTION_DISABLE;

	HAL_I2S_Init( &i2s_handle );

	i2s_dma.Instance                  = I2S_DMA_INSTANCE;
    i2s_dma.Init.Request              = I2S_DMA_REQUEST;
    i2s_dma.Init.Direction            = DMA_PERIPH_TO_MEMORY;
    i2s_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    i2s_dma.Init.MemInc               = DMA_MINC_ENABLE;
    i2s_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    i2s_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
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

    // HAL_I2S_Receive_DMA( &i2s_handle, i2s_buffer, sizeof(i2s_buffer) );
    HAL_I2S_Receive( &i2s_handle, i2s_buffer, sizeof(i2s_buffer), 1000 );
}





