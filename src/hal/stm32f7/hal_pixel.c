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

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"

static uint16_t pix_count_1;

KV_SECTION_META kv_meta_t hal_pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_count_1,        0,    "pix_count_1" },
};


static SPI_HandleTypeDef pix_spi0;
static SPI_HandleTypeDef pix_spi1;
static DMA_HandleTypeDef pix0_dma;
static DMA_HandleTypeDef pix1_dma;


void PIX0_DMA_HANDLER(void){
    
    HAL_DMA_IRQHandler( &pix0_dma );
}

void PIX0_SPI_HANDLER(void){

    HAL_SPI_IRQHandler( &pix_spi0 );
}

void PIX1_DMA_HANDLER(void){
    
    HAL_DMA_IRQHandler( &pix1_dma );
}

void PIX1_SPI_HANDLER(void){

    HAL_SPI_IRQHandler( &pix_spi1 );
}


void HAL_SPI_TxCpltCallback( SPI_HandleTypeDef *hspi ){

    uint8_t driver = 0;

    if( hspi == &pix_spi0 ){

        driver = 0;
    }
    else if( hspi == &pix_spi1 ){

        driver = 1;
    }

    hal_pixel_v_transfer_complete_callback( driver );    
}


uint8_t hal_pixel_u8_driver_count( void ){

    return N_PIXEL_OUTPUTS;
}

uint16_t hal_pixel_u16_driver_pixels( uint8_t driver ){

}

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
    if( driver == 0 ){
        
        HAL_SPI_Transmit_DMA( &pix_spi0, data, len );
    }
    else if( driver == 1 ){

        HAL_SPI_Transmit_DMA( &pix_spi1, data, len );   
    }
    else{

        ASSERT( FALSE );
    }
}

void hal_pixel_v_init( void ){

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    // init IO pins
    GPIO_InitTypeDef GPIO_InitStruct;   
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    // GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = PIX_CLK_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(PIX_CLK_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_GPIO_Port, PIX_CLK_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(PIX_DAT_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_GPIO_Port, PIX_DAT_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_CLK2_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(PIX_CLK2_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK2_GPIO_Port, PIX_CLK2_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT2_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(PIX_DAT2_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT2_GPIO_Port, PIX_DAT2_Pin, GPIO_PIN_RESET);


    pix_spi0.Instance = PIX0_SPI;
    pix_spi1.Instance = PIX1_SPI;

    SPI_InitTypeDef spi_init;
    spi_init.Mode               = SPI_MODE_MASTER;
    spi_init.Direction          = SPI_DIRECTION_2LINES;
    spi_init.DataSize           = SPI_DATASIZE_8BIT;
    spi_init.CLKPolarity        = SPI_POLARITY_LOW;
    spi_init.CLKPhase           = SPI_PHASE_1EDGE;
    spi_init.NSS                = SPI_NSS_SOFT;
    spi_init.BaudRatePrescaler  = SPI_BAUDRATEPRESCALER_32;
    spi_init.FirstBit           = SPI_FIRSTBIT_MSB;
    spi_init.TIMode             = SPI_TIMODE_DISABLE;
    spi_init.CRCCalculation     = SPI_CRCCALCULATION_DISABLE;
    spi_init.CRCPolynomial      = SPI_CRC_LENGTH_DATASIZE;
    spi_init.CRCLength          = 0;
    spi_init.NSSPMode           = SPI_NSS_PULSE_DISABLE;

    // output 0
    // SPI1
    pix_spi0.Init = spi_init;
    HAL_SPI_Init( &pix_spi0 );    

    // output 1
    // SPI2
    pix_spi1.Init = spi_init;
    HAL_SPI_Init( &pix_spi1 );    

    // set up DMA, output 0
    pix0_dma.Instance                  = PIX0_DMA_INSTANCE;
    pix0_dma.Init.Channel              = PIX0_DMA_CHANNEL;
    pix0_dma.Init.Direction            = DMA_MEMORY_TO_PERIPH;
    pix0_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    pix0_dma.Init.MemInc               = DMA_MINC_ENABLE;
    pix0_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    pix0_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    pix0_dma.Init.Mode                 = DMA_NORMAL;
    pix0_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    pix0_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    pix0_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    pix0_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    pix0_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_DMA_Init( &pix0_dma );

    HAL_NVIC_SetPriority( PIX0_DMA_IRQ, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX0_DMA_IRQ );

    HAL_NVIC_SetPriority( PIX0_SPI_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX0_SPI_IRQn );

    __HAL_LINKDMA( &pix_spi0, hdmatx, pix0_dma );

    // set up DMA, output 1
    pix1_dma.Instance                  = PIX1_DMA_INSTANCE;
    pix1_dma.Init.Channel              = PIX1_DMA_CHANNEL;
    pix1_dma.Init.Direction            = DMA_MEMORY_TO_PERIPH;
    pix1_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    pix1_dma.Init.MemInc               = DMA_MINC_ENABLE;
    pix1_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    pix1_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    pix1_dma.Init.Mode                 = DMA_NORMAL;
    pix1_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    pix1_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    pix1_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    pix1_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    pix1_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_DMA_Init( &pix1_dma );

    HAL_NVIC_SetPriority( PIX1_DMA_IRQ, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX1_DMA_IRQ );

    HAL_NVIC_SetPriority( PIX1_SPI_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX1_SPI_IRQn );

    __HAL_LINKDMA( &pix_spi1, hdmatx, pix1_dma );


}

