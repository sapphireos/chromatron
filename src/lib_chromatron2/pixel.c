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

#define FADE_TIMER_VALUE            1 // 1 ms
#define FADE_TIMER_VALUE_PIXIE      2 // Pixie needs at least 1 ms between frames
#define FADE_TIMER_VALUE_WS2811     1 // 1 ms
#define FADE_TIMER_LOW_POWER        20 // 20 ms

#define MAX_BYTES_PER_PIXEL 16

#define HEADER_LENGTH       2
#define TRAILER_LENGTH      32
#define ZERO_PADDING (N_PIXEL_OUTPUTS * (TRAILER_LENGTH + HEADER_LENGTH))

static bool pix_dither;
static uint8_t pix_mode;
static uint8_t pix_mode5;

static uint8_t pix_clock;
static uint8_t pix_rgb_order;
static uint8_t pix_rgb_order5 = PIX_ORDER_GBR;
static uint8_t pix_apa102_dimmer = 31;

static uint8_t array_r[MAX_PIXELS];
static uint8_t array_g[MAX_PIXELS];
static uint8_t array_b[MAX_PIXELS];
static union{
    uint8_t dither[MAX_PIXELS];
    uint8_t white[MAX_PIXELS];
} array_misc;

static uint8_t dither_cycle;

static uint8_t outputs[MAX_PIXELS * MAX_BYTES_PER_PIXEL + ZERO_PADDING];

static uint16_t offsets[N_PIXEL_OUTPUTS];


int8_t pix_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        if( hash == __KV__pix_apa102_dimmer ){

            // bounds check the apa102 dimmer
            if( pix_apa102_dimmer > 31 ){

                pix_apa102_dimmer = 31;
            }
        }
        else{

            // reset pixel drivers
            // pixel_v_init();
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_rgb_order,       0,                    "pix_rgb_order" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_clock,           pix_i8_kv_handler,    "pix_clock" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_PERSIST,                 &pix_dither,          0,                    "pix_dither" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_mode,            pix_i8_kv_handler,    "pix_mode" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_mode5,           pix_i8_kv_handler,    "pix_mode5" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};

static const uint8_t ws2811_lookup[256][4] = {
    #include "ws2811_lookup.txt"
};

static uint8_t bytes_per_pixel( uint8_t mode ){

    if( mode == PIX_MODE_APA102 ){

        return 4; // APA102
    }
    else if( mode == PIX_MODE_WS2811 ){

        return 12; // WS2811
    }
    else if( mode == PIX_MODE_SK6812_RGBW ){

        return 16; // SK6812 RGBW
    }

    return 3; // WS2801 and others
}


static uint16_t setup_pixel_buffer( uint8_t driver, uint8_t **offset ){

    uint8_t *buf = outputs;

    uint8_t driver_pix_mode = pix_mode;
    if( driver == 5 ){

        driver_pix_mode = pix_mode5;

        // return 0;
    }

    // advance buffer for driver
    uint16_t driver_offset = hal_pixel_u16_driver_offset( driver );
    
    buf += offsets[driver];

    *offset = buf;

    uint16_t transfer_pixel_count = hal_pixel_u16_driver_pixels( driver );

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

    */

    uint8_t r, g, b, dither;
    uint8_t rd, gd, bd;

    for( uint16_t i = 0; i < transfer_pixel_count; i++ ){

        r = array_r[i + driver_offset];
        g = array_g[i + driver_offset];
        b = array_b[i + driver_offset];

        if( driver_pix_mode == PIX_MODE_SK6812_RGBW ){

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

        if( driver_pix_mode == PIX_MODE_APA102 ){

            buf[buf_index++] = 0xe0 | pix_apa102_dimmer; // APA102 global brightness control
        }

        uint8_t data0, data1, data2;

        uint8_t rgb_order = pix_rgb_order;
        if( driver == 5 ){

            rgb_order = pix_rgb_order5;
        }

        if( rgb_order == PIX_ORDER_RGB ){

            data0 = r;
            data1 = g;
            data2 = b;
        }
        else if( rgb_order == PIX_ORDER_RBG ){

            data0 = r;
            data1 = b;
            data2 = g;
        }
        else if( rgb_order == PIX_ORDER_GRB ){

            data0 = g;
            data1 = r;
            data2 = b;
        }
        else if( rgb_order == PIX_ORDER_BGR ){

            data0 = b;
            data1 = g;
            data2 = r;
        }
        else if( rgb_order == PIX_ORDER_BRG ){

            data0 = b;
            data1 = r;
            data2 = g;
        }
        else if( rgb_order == PIX_ORDER_GBR ){

            data0 = g;
            data1 = b;
            data2 = r;
        }
        
        if( ( driver_pix_mode == PIX_MODE_WS2811 ) ||
            ( driver_pix_mode == PIX_MODE_SK6812_RGBW ) ){

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

            if( driver_pix_mode == PIX_MODE_SK6812_RGBW ){

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

    hal_cpu_v_clean_d_cache();

    return buf_index;
}


void pixel_v_enable( void ){
   
}

void pixel_v_disable( void ){
   
}


static volatile uint16_t channels_complete;

void hal_pixel_v_transfer_complete_callback( uint8_t driver ){

    ATOMIC;
    channels_complete |= ( 1 << driver );
    END_ATOMIC;

    thread_v_signal( PIX_SIGNAL_0 );
}


PT_THREAD( pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // signal transfer thread to start
    thread_v_signal( PIX_SIGNAL_0 );

    ATOMIC;
    channels_complete = 0xffff;
    END_ATOMIC;

    while(1){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

        ATOMIC;
        uint16_t temp_channels_complete = channels_complete;
        channels_complete = 0;
        END_ATOMIC;

        uint16_t *h = gfx_u16p_get_hue();
        uint16_t *s = gfx_u16p_get_sat();
        uint16_t *v = gfx_u16p_get_val();
        uint16_t r, g, b, w;

        for( uint8_t ch = 0; ch < hal_pixel_u8_driver_count(); ch++ ){

            if( temp_channels_complete & ( 1 << ch ) ){

                uint16_t pix_offset = hal_pixel_u16_driver_offset( ch );
                uint16_t pix_count = hal_pixel_u16_driver_pixels( ch );

                uint8_t driver_pix_mode = pix_mode;
                if( ch == 5 ){

                    driver_pix_mode = pix_mode5;
                }

                // run HSV to RGBW conversion for this channel
                for( uint32_t i = pix_offset; i < pix_count + pix_offset; i++ ){

                    uint16_t dimmed_val = gfx_u16_get_dimmed_val( v[i] );


                    // if( driver_pix_mode == PIX_MODE_SK6812_RGBW ){

                    //     gfx_v_hsv_to_rgbw( h[i], s[i], dimmed_val, &r, &g, &b, &w );
                        
                    //     array_r[i] = r >> 8;
                    //     array_g[i] = g >> 8;
                    //     array_b[i] = b >> 8;
                    //     array_misc.white[i] = w >> 8;
                    // }
                    // else{

                        gfx_v_hsv_to_rgb( h[i], s[i], dimmed_val, &r, &g, &b );

                        array_r[i] = r >> 8;
                        array_g[i] = g >> 8;
                        array_b[i] = b >> 8;
                        array_misc.dither[i] = 0;
                    // }
                }

                uint8_t *offset;
                uint16_t data_length = setup_pixel_buffer( ch, &offset );

                hal_pixel_v_start_transfer( ch, offset, data_length );
            }
        }

        TMR_WAIT( pt, 2 ); 
    }

PT_END( pt );
}


void pixel_v_init( void ){

    hal_pixel_v_init();

    pixel_v_enable();

    offsets[0] = 0;

    for( uint8_t ch = 1; ch < hal_pixel_u8_driver_count(); ch++ ){

        uint8_t driver_pix_mode = pix_mode;
        // if( ch == 5 ){

        //     driver_pix_mode = pix_mode5;
        // }

        offsets[ch] = offsets[ch - 1] + 
                            hal_pixel_u16_driver_pixels( ch - 1 ) * bytes_per_pixel( driver_pix_mode ) + 
                            TRAILER_LENGTH + HEADER_LENGTH;
    }

    thread_t_create( pixel_thread,
                     PSTR("pixel"),
                     0,
                     0 );
}

bool pixel_b_enabled( void ){

    return 0;
}

uint8_t pixel_u8_get_mode( void ){

    return 0;
}

