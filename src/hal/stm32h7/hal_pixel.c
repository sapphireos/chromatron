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

// H7

/*

Channel - Port      - DMA
0         SPI 1       1
1         SPI 2       1
2         SPI 3       1
3         SPI 4       1
4         SPI 5       2
5         SPI 6       2
6         USART 1
7         USART 2
8         UART 4
9         UART 5

*/

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"

#include "logging.h"

static uint16_t pix_counts[N_PIXEL_OUTPUTS];

KV_SECTION_META kv_meta_t hal_pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[0],        0,    "pix_count" },
    #ifdef BOARD_CHROMATRONX
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[1],        0,    "pix_count_1" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[2],        0,    "pix_count_2" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[3],        0,    "pix_count_3" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[4],        0,    "pix_count_4" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[5],        0,    "pix_count_5" },
    #endif
};


static SPI_HandleTypeDef    pix_spi0;

#ifdef BOARD_CHROMATRONX
static SPI_HandleTypeDef    pix_spi1;
static SPI_HandleTypeDef    pix_spi2;
static SPI_HandleTypeDef    pix_spi3;
static SPI_HandleTypeDef    pix_spi4;
static SPI_HandleTypeDef    pix_spi5;
#endif

static DMA_HandleTypeDef pix0_dma;

#ifdef BOARD_CHROMATRONX
static DMA_HandleTypeDef pix1_dma;
static DMA_HandleTypeDef pix2_dma;
static DMA_HandleTypeDef pix3_dma;
static DMA_HandleTypeDef pix4_dma;
// static DMA_HandleTypeDef pix5_dma;

static uint8_t p5_buf[128];
static uint8_t p5_count;
#endif


ISR(PIX0_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &pix0_dma );
}

ISR(PIX0_SPI_HANDLER){

    HAL_SPI_IRQHandler( &pix_spi0 );
}

#ifdef BOARD_CHROMATRONX
ISR(PIX1_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &pix1_dma );
}

ISR(PIX1_SPI_HANDLER){

    HAL_SPI_IRQHandler( &pix_spi1 );
}


ISR(PIX2_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &pix2_dma );
}

ISR(PIX2_SPI_HANDLER){

    HAL_SPI_IRQHandler( &pix_spi2 );
}

ISR(PIX3_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &pix3_dma );
}

ISR(PIX3_SPI_HANDLER){

    HAL_SPI_IRQHandler( &pix_spi3 );
}

ISR(PIX4_DMA_HANDLER){
    
    HAL_DMA_IRQHandler( &pix4_dma );
}

ISR(PIX4_SPI_HANDLER){

    HAL_SPI_IRQHandler( &pix_spi4 );
}

ISR(PIX5_SPI_HANDLER){

    HAL_SPI_IRQHandler( &pix_spi5 );
}

#endif

void HAL_SPI_TxCpltCallback( SPI_HandleTypeDef *hspi ){

    uint8_t driver = 255;

    if( hspi == &pix_spi0 ){

        driver = 0;
    }
    #ifdef BOARD_CHROMATRONX
    else if( hspi == &pix_spi1 ){

        driver = 1;
    }
    else if( hspi == &pix_spi2 ){

        driver = 2;
    }
    else if( hspi == &pix_spi3 ){

        driver = 3;
    }
    else if( hspi == &pix_spi4 ){

        driver = 4;

        HAL_SPI_Transmit( &pix_spi5, p5_buf, p5_count, 50 );
        hal_pixel_v_transfer_complete_callback( 5 ); 
    }
    #endif
        
    if( driver < N_PIXEL_OUTPUTS ){

        hal_pixel_v_transfer_complete_callback( driver );        
    }
}


uint8_t hal_pixel_u8_driver_count( void ){

    return N_PIXEL_OUTPUTS;
}

uint16_t hal_pixel_u16_driver_pixels( uint8_t driver ){

    ASSERT( driver < N_PIXEL_OUTPUTS );
    
    return pix_counts[driver];
}

uint16_t hal_pixel_u16_driver_offset( uint8_t driver ){

    ASSERT( driver < N_PIXEL_OUTPUTS );

    uint16_t offset = 0;

    for( uint8_t i = 0; i < driver; i++ ){

        offset += pix_counts[i];
    }
    
    return offset;
}

uint16_t hal_pixel_u16_get_pix_count( void ){

    uint16_t count = 0;

    for( uint8_t i = 0; i < N_PIXEL_OUTPUTS; i++ ){

        count += pix_counts[i];
    }

    return count;
}

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
    if( driver == 0 ){
        
        HAL_SPI_Transmit_DMA( &pix_spi0, data, len );
    }
    #ifdef BOARD_CHROMATRONX
    else if( driver == 1 ){

        HAL_SPI_Transmit_DMA( &pix_spi1, data, len );   
    }
    else if( driver == 2 ){

        HAL_SPI_Transmit_DMA( &pix_spi2, data, len );   
    }
    else if( driver == 3 ){

        HAL_SPI_Transmit_DMA( &pix_spi3, data, len ); 
    }
    else if( driver == 4 ){

        HAL_SPI_Transmit_DMA( &pix_spi4, data, len );   
    }
    else if( driver == 5 ){

        // note driver 5 does not use DMA!
            
        // uint8_t buf[46];
        // memset( buf, 0, sizeof(buf) );
        memcpy(p5_buf, data, len);
        p5_count = len;

        // // ATOMIC;
        // // HAL_SPI_Transmit( &pix_spi5, data, len, 50 );
        // HAL_SPI_Transmit( &pix_spi5, buf, 46, 50 );
        // // END_ATOMIC;

        // hal_pixel_v_transfer_complete_callback( 5 ); 
    }
    #endif
    else{

        ASSERT( FALSE );
    }
}

#ifndef BOARD_CHROMATRONX
typedef struct{
    uint32_t pin;
    uint32_t alt;
} hal_pix_ch_t;

static const hal_pix_ch_t pix_io[] = {
    { IO_PIN_PIX_CLK, GPIO_AF5_SPI1 },
    { IO_PIN_PIX_DAT, GPIO_AF5_SPI1 },
};
#endif


void hal_pixel_v_init( void ){

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    __HAL_RCC_SPI1_CLK_ENABLE();

    #ifdef BOARD_CHROMATRONX
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();
    __HAL_RCC_SPI4_CLK_ENABLE();
    __HAL_RCC_SPI5_CLK_ENABLE();
    __HAL_RCC_SPI6_CLK_ENABLE();
    #endif

    // NOTE! SPI clocks are set to use PLL2!
    // See hal_cpu.c for details.  This should be 104 MHz.

    // init IO pins
    
    GPIO_InitTypeDef GPIO_InitStruct;   
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    // GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    // NOTE the alternate port functions are NOT all the same!

    #ifndef BOARD_CHROMATRONX
    GPIO_TypeDef *port;
    
    hal_io_v_get_port( pix_io[0].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = pix_io[0].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );
    HAL_GPIO_WritePin(port, GPIO_InitStruct.Pin, GPIO_PIN_RESET);
    
    hal_io_v_get_port( pix_io[1].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = pix_io[1].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );
    HAL_GPIO_WritePin(port, GPIO_InitStruct.Pin, GPIO_PIN_RESET);
    
    #else

    GPIO_InitStruct.Pin = PIX_CLK_0_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(PIX_CLK_0_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_0_GPIO_Port, PIX_CLK_0_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_0_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(PIX_DAT_0_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_0_GPIO_Port, PIX_DAT_0_Pin, GPIO_PIN_RESET);
    


    GPIO_InitStruct.Pin = PIX_CLK_1_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(PIX_CLK_1_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_1_GPIO_Port, PIX_CLK_1_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_1_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(PIX_DAT_1_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_1_GPIO_Port, PIX_DAT_1_Pin, GPIO_PIN_RESET);


    GPIO_InitStruct.Pin = PIX_CLK_2_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(PIX_CLK_2_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_2_GPIO_Port, PIX_CLK_2_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_2_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF7_SPI3;
    HAL_GPIO_Init(PIX_DAT_2_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_2_GPIO_Port, PIX_DAT_2_Pin, GPIO_PIN_RESET);


    GPIO_InitStruct.Pin = PIX_CLK_3_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI4;
    HAL_GPIO_Init(PIX_CLK_3_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_3_GPIO_Port, PIX_CLK_3_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_3_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI4;
    HAL_GPIO_Init(PIX_DAT_3_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_3_GPIO_Port, PIX_DAT_3_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_CLK_4_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
    HAL_GPIO_Init(PIX_CLK_4_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_4_GPIO_Port, PIX_CLK_4_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_4_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
    HAL_GPIO_Init(PIX_DAT_4_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_4_GPIO_Port, PIX_DAT_4_Pin, GPIO_PIN_RESET);


    GPIO_InitStruct.Pin = PIX_CLK_5_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI6;
    HAL_GPIO_Init(PIX_CLK_5_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_CLK_5_GPIO_Port, PIX_CLK_5_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = PIX_DAT_5_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF8_SPI6;
    HAL_GPIO_Init(PIX_DAT_5_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PIX_DAT_5_GPIO_Port, PIX_DAT_5_Pin, GPIO_PIN_RESET);



    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);

    // GPIO_InitStruct.Pin = PIX_CLK_6_Pin;
    // GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    // HAL_GPIO_Init(PIX_CLK_6_GPIO_Port, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(PIX_CLK_6_GPIO_Port, PIX_CLK_6_Pin, GPIO_PIN_RESET);

    // GPIO_InitStruct.Pin = PIX_DAT_6_Pin;
    // GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    // HAL_GPIO_Init(PIX_DAT_6_GPIO_Port, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(PIX_DAT_6_GPIO_Port, PIX_DAT_6_Pin, GPIO_PIN_RESET);


    // GPIO_InitStruct.Pin = PIX_CLK_7_Pin;
    // GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    // HAL_GPIO_Init(PIX_CLK_7_GPIO_Port, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(PIX_CLK_7_GPIO_Port, PIX_CLK_7_Pin, GPIO_PIN_RESET);

    // GPIO_InitStruct.Pin = PIX_DAT_7_Pin;
    // GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    // HAL_GPIO_Init(PIX_DAT_7_GPIO_Port, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(PIX_DAT_7_GPIO_Port, PIX_DAT_7_Pin, GPIO_PIN_RESET);


    // // NOTE channels 8 and 9 do not have clock pins

    // GPIO_InitStruct.Pin = PIX_DAT_8_Pin;
    // GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    // HAL_GPIO_Init(PIX_DAT_8_GPIO_Port, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(PIX_DAT_8_GPIO_Port, PIX_DAT_8_Pin, GPIO_PIN_RESET);

    // GPIO_InitStruct.Pin = PIX_DAT_9_Pin;
    // GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
    // HAL_GPIO_Init(PIX_DAT_9_GPIO_Port, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(PIX_DAT_9_GPIO_Port, PIX_DAT_9_Pin, GPIO_PIN_RESET);
    #endif


    pix_spi0.Instance = PIX0_SPI;
    #ifdef BOARD_CHROMATRONX
    pix_spi1.Instance = PIX1_SPI;
    pix_spi2.Instance = PIX2_SPI;
    pix_spi3.Instance = PIX3_SPI;
    pix_spi4.Instance = PIX4_SPI;
    pix_spi5.Instance = PIX5_SPI;
    #endif

    SPI_InitTypeDef spi_init;
    spi_init.Mode               = SPI_MODE_MASTER;
    spi_init.Direction          = SPI_DIRECTION_2LINES_TXONLY;
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
    spi_init.NSSPolarity                   = SPI_NSS_POLARITY_LOW;
    spi_init.FifoThreshold                 = SPI_FIFO_THRESHOLD_01DATA;
    spi_init.TxCRCInitializationPattern    = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    spi_init.RxCRCInitializationPattern    = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    spi_init.MasterSSIdleness              = SPI_MASTER_SS_IDLENESS_00CYCLE;
    spi_init.MasterInterDataIdleness       = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    spi_init.MasterReceiverAutoSusp        = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    spi_init.MasterKeepIOState             = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    // NOTE! if MasterKeepIOState is disabled, the SPI will TRISTATE the IO lines
    // when not in use!!!
    spi_init.IOSwap                        = SPI_IO_SWAP_DISABLE;

    // output 0
    // SPI1
    pix_spi0.Init = spi_init;
    HAL_SPI_Init( &pix_spi0 );    

    #ifdef BOARD_CHROMATRONX
    // output 1
    // SPI2
    pix_spi1.Init = spi_init;
    HAL_SPI_Init( &pix_spi1 );    

    // output 2
    // SPI3
    pix_spi2.Init = spi_init;
    HAL_SPI_Init( &pix_spi2 );    

    // output 3
    // SPI4
    pix_spi3.Init = spi_init;
    HAL_SPI_Init( &pix_spi3 );    

    // output 4
    // SPI5
    pix_spi4.Init = spi_init;
    HAL_SPI_Init( &pix_spi4 );    

    // output 5
    // SPI6
    pix_spi5.Init = spi_init;
    HAL_SPI_Init( &pix_spi5 );    
    #endif

    // set up DMA, output 0
    pix0_dma.Instance                  = PIX0_DMA_INSTANCE;
    pix0_dma.Init.Request              = PIX0_DMA_REQUEST;
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

    #ifdef BOARD_CHROMATRONX
    // set up DMA, output 1
    pix1_dma.Instance                  = PIX1_DMA_INSTANCE;
    pix1_dma.Init.Request              = PIX1_DMA_REQUEST;
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


    // set up DMA, output 2
    pix2_dma.Instance                  = PIX2_DMA_INSTANCE;
    pix2_dma.Init.Request              = PIX2_DMA_REQUEST;
    pix2_dma.Init.Direction            = DMA_MEMORY_TO_PERIPH;
    pix2_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    pix2_dma.Init.MemInc               = DMA_MINC_ENABLE;
    pix2_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    pix2_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    pix2_dma.Init.Mode                 = DMA_NORMAL;
    pix2_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    pix2_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    pix2_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    pix2_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    pix2_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_DMA_Init( &pix2_dma );

    HAL_NVIC_SetPriority( PIX2_DMA_IRQ, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX2_DMA_IRQ );

    HAL_NVIC_SetPriority( PIX2_SPI_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX2_SPI_IRQn );

    __HAL_LINKDMA( &pix_spi2, hdmatx, pix2_dma );


    // set up DMA, output 3
    pix3_dma.Instance                  = PIX3_DMA_INSTANCE;
    pix3_dma.Init.Request              = PIX3_DMA_REQUEST;
    pix3_dma.Init.Direction            = DMA_MEMORY_TO_PERIPH;
    pix3_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    pix3_dma.Init.MemInc               = DMA_MINC_ENABLE;
    pix3_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    pix3_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    pix3_dma.Init.Mode                 = DMA_NORMAL;
    pix3_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    pix3_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    pix3_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    pix3_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    pix3_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_DMA_Init( &pix3_dma );

    HAL_NVIC_SetPriority( PIX3_DMA_IRQ, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX3_DMA_IRQ );

    HAL_NVIC_SetPriority( PIX3_SPI_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX3_SPI_IRQn );

    __HAL_LINKDMA( &pix_spi3, hdmatx, pix3_dma );


    // set up DMA, output 4
    pix4_dma.Instance                  = PIX4_DMA_INSTANCE;
    pix4_dma.Init.Request              = PIX4_DMA_REQUEST;
    pix4_dma.Init.Direction            = DMA_MEMORY_TO_PERIPH;
    pix4_dma.Init.PeriphInc            = DMA_PINC_DISABLE;
    pix4_dma.Init.MemInc               = DMA_MINC_ENABLE;
    pix4_dma.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    pix4_dma.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    pix4_dma.Init.Mode                 = DMA_NORMAL;
    pix4_dma.Init.Priority             = DMA_PRIORITY_HIGH;
    pix4_dma.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    pix4_dma.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_1QUARTERFULL; 
    pix4_dma.Init.MemBurst             = DMA_MBURST_SINGLE;
    pix4_dma.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    HAL_DMA_Init( &pix4_dma );

    HAL_NVIC_SetPriority( PIX4_DMA_IRQ, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX4_DMA_IRQ );

    HAL_NVIC_SetPriority( PIX4_SPI_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( PIX4_SPI_IRQn );

    __HAL_LINKDMA( &pix_spi4, hdmatx, pix4_dma );
    #endif
}

