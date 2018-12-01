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

#define FADE_TIMER_VALUE            1 // 1 ms
#define FADE_TIMER_VALUE_PIXIE      2 // Pixie needs at least 1 ms between frames
#define FADE_TIMER_VALUE_WS2811     1 // 1 ms
#define FADE_TIMER_LOW_POWER        20 // 20 ms


#define MAX_BYTES_PER_PIXEL 16

static bool pix_dither;
static uint16_t pix_count;
static uint8_t pix_mode;

static uint8_t pix_clock;
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

static uint8_t outputs[N_PIXEL_OUTPUTS][MAX_PIXELS * MAX_BYTES_PER_PIXEL];


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
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST,                 &pix_count,           pix_i8_kv_handler,    "pix_count" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_clock,           pix_i8_kv_handler,    "pix_clock" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_PERSIST,                 &pix_dither,          0,                    "pix_dither" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_mode,            pix_i8_kv_handler,    "pix_mode" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};

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

    memset( &buf[buf_index], 0, 32 );
    buf_index += 32;

    hal_cpu_v_clean_d_cache();

    return buf_index;
}


void pixel_v_enable( void ){
   
}

void pixel_v_disable( void ){
   
}

void pixel_v_set_analog_rgb( uint16_t r, uint16_t g, uint16_t b ){

}




static volatile uint16_t channels_complete;

void hal_pixel_v_transfer_complete_callback( uint8_t driver ){

    channels_complete |= ( 1 << driver );
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

        for( uint8_t ch = 0; ch < N_PIXEL_OUTPUTS; ch++ ){

            if( temp_channels_complete & ( 1 << ch ) ){

                uint16_t *h = gfx_u16p_get_hue();
                uint16_t *s = gfx_u16p_get_sat();
                uint16_t *v = gfx_u16p_get_val();
                uint16_t r, g, b, w;

                for( uint32_t i = 0; i < gfx_u16_get_pix_count(); i++ ){

                    gfx_v_hsv_to_rgbw( *h, *s, *v, &r, &g, &b, &w );

                    array_r[i] = r >> 8;
                    array_g[i] = g >> 8;
                    array_b[i] = b >> 8;
                    array_misc.white[i] = w >> 8;

                    h++;
                    s++;
                    v++;
                }

                uint16_t data_length = setup_pixel_buffer( outputs[ch], sizeof(outputs[ch]) );

                hal_pixel_v_start_transfer( ch, outputs[ch], data_length );
            }
        }

        TMR_WAIT( pt, 2 ); 
    }

PT_END( pt );
}


void pixel_v_init( void ){

    pix_mode = PIX_MODE_SK6812_RGBW;

    hal_pixel_v_init();

    pixel_v_enable();

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

