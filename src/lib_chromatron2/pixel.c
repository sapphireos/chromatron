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


#include "sapphire.h"

#include "config.h"
#include "pixel.h"
#include "graphics.h"
#include "hsv_to_rgb.h"
#include "pwm.h"
#include "hal_usart.h"

#include "os_irq.h"

#include "logging.h"
#include "hal_pixel.h"

#include <math.h>

static SPI_HandleTypeDef pix_spi0;
static SPI_HandleTypeDef pix_spi1;
static DMA_HandleTypeDef pix0_dma;


#define MAX_BYTES_PER_PIXEL 16

static bool pix_dither;
static uint8_t pix_mode;

static uint8_t pix_rgb_order;
static uint8_t pix_apa102_dimmer = 31;

static uint8_t array_r[MAX_PIXELS];
static uint8_t array_g[MAX_PIXELS];
static uint8_t array_b[MAX_PIXELS];
static union{
    uint8_t dither[MAX_PIXELS];
    uint8_t white[MAX_PIXELS];
} array_misc;

static uint8_t dither_cycle;

static uint8_t output0[MAX_PIXELS * MAX_BYTES_PER_PIXEL];

static const uint8_t ws2811_lookup[256][4] = {
    #include "ws2811_lookup.txt"
};

static uint8_t bytes_per_pixel( void ){

    if( pix_mode == PIX_MODE_APA102 ){

        return 4; // APA102
    }
    else if( pix_mode == PIX_MODE_WS2811 ){

        return 12; // WS2811
    }
    else if( pix_mode == PIX_MODE_SK6812_RGBW ){

        return 16; // SK6812 RGBW
    }

    return 3; // WS2801 and others
}


static uint8_t setup_pixel_buffer( uint8_t *buf, uint16_t len ){

    uint16_t pixels_per_buf = len / bytes_per_pixel();
    uint16_t transfer_pixel_count = pixels_per_buf;
    uint16_t pixels_remaining = gfx_u16_get_pix_count();

    if( transfer_pixel_count > pixels_remaining ){

        transfer_pixel_count = pixels_remaining;
    }

    if( transfer_pixel_count == 0 ){

        return 0;
    }

    uint16_t buf_index = 0;

    uint8_t r, g, b, dither;
    uint8_t rd, gd, bd;

    for( uint16_t i = 0; i < transfer_pixel_count; i++ ){

        r = array_r[i];
        g = array_g[i];
        b = array_b[i];

        if( pix_mode == PIX_MODE_SK6812_RGBW ){

        }
        else if( pix_dither ){

            dither = array_misc.dither[i];

            rd = ( dither >> 4 ) & 0x03;
            gd = ( dither >> 2 ) & 0x03;
            bd = ( dither >> 0 ) & 0x03;

            if( ( r < 255 ) && ( rd > ( dither_cycle & 0x03 ) ) ){

                r++;
            }

            if( ( g < 255 ) && ( gd > ( dither_cycle & 0x03 ) ) ){

                g++;
            }

            if( ( b < 255 ) && ( bd > ( dither_cycle & 0x03 ) ) ){

                b++;
            }
        }

        if( pix_mode == PIX_MODE_APA102 ){

            buf[buf_index++] = 0xe0 | pix_apa102_dimmer; // APA102 global brightness control
        }

        uint8_t data0, data1, data2;

        if( pix_rgb_order == PIX_ORDER_RGB ){

            data0 = r;
            data1 = g;
            data2 = b;
        }
        else if( pix_rgb_order == PIX_ORDER_RBG ){

            data0 = r;
            data1 = b;
            data2 = g;
        }
        else if( pix_rgb_order == PIX_ORDER_GRB ){

            data0 = g;
            data1 = r;
            data2 = b;
        }
        else if( pix_rgb_order == PIX_ORDER_BGR ){

            data0 = b;
            data1 = g;
            data2 = r;
        }
        else if( pix_rgb_order == PIX_ORDER_BRG ){

            data0 = b;
            data1 = r;
            data2 = g;
        }
        else if( pix_rgb_order == PIX_ORDER_GBR ){

            data0 = g;
            data1 = b;
            data2 = r;
        }
        
        if( ( pix_mode == PIX_MODE_WS2811 ) ||
            ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

            // ws2811 bitstream lookup

            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data0][0] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data0][1] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data0][2] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data0][3] );

            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data1][0] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data1][1] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data1][2] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data1][3] );

            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data2][0] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data2][1] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data2][2] );
            buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[data2][3] );

            if( pix_mode == PIX_MODE_SK6812_RGBW ){

                uint8_t white = array_misc.white[i];

                buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[white][0] );
                buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[white][1] );
                buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[white][2] );
                buf[buf_index++] = ~pgm_read_byte( &ws2811_lookup[white][3] );
            }
        }
        else{

            buf[buf_index++] = data0;
            buf[buf_index++] = data1;
            buf[buf_index++] = data2;
        }
    }

    return buf_index;
}


void pixel_v_enable( void ){
   
}

void pixel_v_disable( void ){
   
}

void pixel_v_set_analog_rgb( uint16_t r, uint16_t g, uint16_t b ){

}

void pixel_v_init( void ){

    pix_mode = PIX_MODE_SK6812_RGBW;

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
    spi_init.CRCLength          = SPI_RXFIFO_THRESHOLD;
    spi_init.NSSPMode           = SPI_NSS_PULSE_DISABLE;

    // output 0
    // SPI1
    pix_spi0.Init = spi_init;
    HAL_SPI_Init( &pix_spi0 );    

    // output 1
    // SPI2
    pix_spi1.Init = spi_init;
    HAL_SPI_Init( &pix_spi1 );    

    // set up DMA
    pix0_dma.Instance                  = DMA2_Stream3;
    pix0_dma.Init.Channel              = DMA_CHANNEL_3;
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

    __HAL_LINKDMA( &pix_spi0, hdmatx, pix0_dma );

    // uint8_t data = 0x43;

    // HAL_SPI_Transmit( &pix_spi0, &data, sizeof(data), 100 );

    // HAL_SPI_Transmit( &pix_spi1, &data, sizeof(data), 100 );

    memset(array_r, 0x11, sizeof(array_r));
    array_r[2] = 0xff;
    array_g[3] = 0xff;
    array_b[4] = 0xff;

    uint8_t zeros[32];
    memset(zeros, 0, sizeof(zeros));

    setup_pixel_buffer(output0, sizeof(output0));


    HAL_SPI_Transmit( &pix_spi0, zeros, sizeof(zeros), 100 );
    // HAL_SPI_Transmit( &pix_spi0, output0, sizeof(output0), 100 );
    // HAL_SPI_Transmit( &pix_spi0, output0, 12 * 8, 100 );
    HAL_SPI_Transmit_DMA( &pix_spi0, output0, 12 * 8 );

    // HAL_SPI_Transmit( &pix_spi1, output0, sizeof(output0), 100 );
    HAL_SPI_Transmit( &pix_spi0, zeros, sizeof(zeros), 100 );

    pixel_v_enable();
}

bool pixel_b_enabled( void ){

    return 0;
}

uint8_t pixel_u8_get_mode( void ){

    return 0;
}

void pixel_v_load_rgb(
    uint16_t index,
    uint16_t len,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *d ){

}

void pixel_v_load_rgb16(
    uint16_t index,
    uint16_t r,
    uint16_t g,
    uint16_t b ){

}

void pixel_v_load_hsv(
    uint16_t index,
    uint16_t len,
    uint16_t *h,
    uint16_t *s,
    uint16_t *v ){

}

void pixel_v_get_rgb_totals( uint16_t *r, uint16_t *g, uint16_t *b ){

}

