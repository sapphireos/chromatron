// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

// ESP8266

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"
#include "os_irq.h"
#include "spi.h"

#include "pixel.h"
#include "pixel_vars.h"
#include "gfx_lib.h"

#include "logging.h"

#define FADE_TIMER_VALUE            1 // 1 ms
#define FADE_TIMER_VALUE_PIXIE      2 // Pixie needs at least 1 ms between frames
#define FADE_TIMER_VALUE_WS2811     1 // 1 ms
#define FADE_TIMER_LOW_POWER        20 // 20 ms

#define MAX_BYTES_PER_PIXEL 16

#define HEADER_LENGTH       2
#define TRAILER_LENGTH      32
#define ZERO_PADDING (N_PIXEL_OUTPUTS * (TRAILER_LENGTH + HEADER_LENGTH))

#ifdef PIXEL_USE_MALLOC

static uint8_t *array_r;
static uint8_t *array_g;
static uint8_t *array_b;
static union{
    uint8_t *dither;
    uint8_t *white;
} array_misc;
static uint8_t *outputs;

#else

static uint8_t array_r[MAX_PIXELS];
static uint8_t array_g[MAX_PIXELS];
static uint8_t array_b[MAX_PIXELS];
static union{
    uint8_t dither[MAX_PIXELS];
    uint8_t white[MAX_PIXELS];
} array_misc;
static uint8_t outputs[MAX_PIXELS * MAX_BYTES_PER_PIXEL + ZERO_PADDING];

#endif

static uint8_t dither_cycle;

static const uint8_t ws2811_lookup[256][4] = {
    #include "ws2811_lookup.txt"
};


static uint16_t setup_pixel_buffer( void ){

    uint8_t *buf = outputs;

    uint16_t transfer_pixel_count = gfx_u16_get_pix_count();

    if( transfer_pixel_count == 0 ){

        return 0;
    }

    uint16_t buf_index = 0;
    buf[buf_index++] = 0; // first byte is 0
    buf[buf_index++] = 0; // second byte is 0
    /*
    Why make the first byte 0?

    Because the STM32 SPI hardware will glitch the data line on the first byte, 
    because it sets up MOSI to high.  It is assuming we are using the clock, so
    normally this wouldn't matter.  But on a WS2812 or SK6812, it glitches the first
    pixel.  So, we send 0 first so the data line doesn't init to 1.


	!!!!!!!!!!!!!!!!!!!!!!!!!

	Need to check this on the ESP32.
	We will leave the 0 header in for now!

	!!!!!!!!!!!!!!!!!!!!!!!!!

    */

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
        else{

        	ASSERT( FALSE );

        	data0 = 0;
        	data1 = 0;
        	data2 = 0;
        }
        
        if( ( pix_mode == PIX_MODE_WS2811 ) ||
            ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

            // ws2811 bitstream lookup

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][2] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][3] );

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][2] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][3] );

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][2] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][3] );

            if( pix_mode == PIX_MODE_SK6812_RGBW ){

                uint8_t white = array_misc.white[i];

                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][0] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][1] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][2] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][3] );
            }
        }
        else{

            buf[buf_index++] = data0;
            buf[buf_index++] = data1;
            buf[buf_index++] = data2;
        }
    }

    memset( &buf[buf_index], 0, TRAILER_LENGTH );
    buf_index += TRAILER_LENGTH;

    return buf_index;
}


PT_THREAD( pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // signal transfer thread to start
    thread_v_signal( PIX_SIGNAL_0 );

    while(1){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

        THREAD_WAIT_WHILE( pt, pix_mode == PIX_MODE_OFF );

        uint16_t *h = gfx_u16p_get_hue();
        uint16_t *s = gfx_u16p_get_sat();
        uint16_t *v = gfx_u16p_get_val();
        uint16_t r, g, b, w;

        // run HSV to RGBW conversion for this channel
        for( uint32_t i = 0; i < gfx_u16_get_pix_count(); i++ ){

            uint16_t dimmed_val = gfx_u16_get_dimmed_val( v[i] );

            if( pix_mode == PIX_MODE_SK6812_RGBW ){

                gfx_v_hsv_to_rgbw( h[i], s[i], dimmed_val, &r, &g, &b, &w );
                
                array_r[i] = r >> 8;
                array_g[i] = g >> 8;
                array_b[i] = b >> 8;
                array_misc.white[i] = w >> 8;
            }
            else{

                gfx_v_hsv_to_rgb( h[i], s[i], dimmed_val, &r, &g, &b );

                array_r[i] = r >> 8;
                array_g[i] = g >> 8;
                array_b[i] = b >> 8;
                array_misc.dither[i] = 0;
            }
        }

        uint16_t data_length = setup_pixel_buffer();
        
        // initiate SPI transfer
        // this is blocking!
        spi_v_write_block( PIXEL_SPI_CHANNEL, outputs, data_length );

        THREAD_YIELD( pt );
    }

PT_END( pt );
}

void hal_pixel_v_init( void ){

    #ifdef PIXEL_USE_MALLOC

    array_r = malloc( MAX_PIXELS );
    array_g = malloc( MAX_PIXELS );
    array_b = malloc( MAX_PIXELS );
    array_misc.white = malloc( MAX_PIXELS );

    outputs = malloc( MAX_PIXELS * MAX_BYTES_PER_PIXEL + ZERO_PADDING );
    
    ASSERT( array_r != 0 );
    ASSERT( array_g != 0 );
    ASSERT( array_b != 0 );
    ASSERT( array_misc.white != 0 );
    ASSERT( outputs != 0 );

    #endif

	thread_t_create( pixel_thread,
                     PSTR("pixel"),
                     0,
                     0 );

	hal_pixel_v_configure();
}

void hal_pixel_v_configure( void ){

	spi_v_init( PIXEL_SPI_CHANNEL, pix_clock, 0 );
}

