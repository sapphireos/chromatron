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

#include <math.h>


static bool pix_dither;
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
        else if( hash == __KV__pix_count ){

            gfx_v_set_pix_count( *(uint16_t *)data );
        }   
        else{

            // reset pixel drivers
            pixel_v_init();
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_rgb_order,       0,                    "pix_rgb_order" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_clock,           pix_i8_kv_handler,    "pix_clock" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_PERSIST,                 &pix_dither,          0,                    "pix_dither" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_mode,            pix_i8_kv_handler,    "pix_mode" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};



void pixel_v_enable( void ){

}

void pixel_v_disable( void ){

}

void pixel_v_set_analog_rgb( uint16_t r, uint16_t g, uint16_t b ){

    if( pix_mode != PIX_MODE_ANALOG ){

        return;
    }

    uint16_t data0, data1, data2;

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

    pwm_v_write( 0, data0 );
    pwm_v_write( 1, data1 );
    pwm_v_write( 2, data2 );
}

void pixel_v_init( void ){

    // bounds check
    if( pix_apa102_dimmer > 31 ){

        pix_apa102_dimmer = 31;
    }

    // disable drivers
    pixel_v_disable();

    if( pix_mode == PIX_MODE_OFF ){

        return;
    }

    if( pix_mode == PIX_MODE_ANALOG ){

        pwm_v_init();

        pwm_v_write( 0, 0 );
        pwm_v_write( 1, 0 );
        pwm_v_write( 2, 0 );
        pwm_v_write( 3, 0 );

        return;
    }

    if( pix_clock < 7 ){

        pix_clock = 31;
    }

    if( ( pix_mode == PIX_MODE_WS2811 ) ||
        ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

        pix_clock = 6; // 2.461 Mhz
    }

    pixel_v_enable();
}

bool pixel_b_enabled( void ){

    return pix_mode != PIX_MODE_OFF;
}

uint8_t pixel_u8_get_mode( void ){

    return pix_mode;
}

void pixel_v_load_rgb(
    uint16_t index,
    uint16_t len,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *d ){

    uint16_t transfer_count = len;

    if( ( index + transfer_count ) > MAX_PIXELS ){

        transfer_count = MAX_PIXELS - index;
    }

    // bounds check
    if( ( index + transfer_count ) > MAX_PIXELS ){

        log_v_debug_P( PSTR("pix transfer out of bounds") );
        return;
    }

    // need to do the copy with interrupts disabled,
    // so that way we have access into the arrays without
    // the pixel driver touching them
    ATOMIC;

    memcpy( &array_r[index], r, transfer_count );
    memcpy( &array_g[index], g, transfer_count );
    memcpy( &array_b[index], b, transfer_count );
    memcpy( &array_misc.dither[index], d, transfer_count );

    END_ATOMIC;
}

void pixel_v_load_rgb16(
    uint16_t index,
    uint16_t r,
    uint16_t g,
    uint16_t b ){

    uint8_t dither;

    r /= 64;
    g /= 64;
    b /= 64;

    dither =  ( r & 0x0003 ) << 4;
    dither |= ( g & 0x0003 ) << 2;
    dither |= ( b & 0x0003 );

    r /= 4;
    g /= 4;
    b /= 4;
    
    // need to do the copy with interrupts disabled,
    // so that way we have access into the arrays without
    // the pixel driver touching them
    ATOMIC;
    array_r[index] = r;
    array_g[index] = g;
    array_b[index] = b;
    array_misc.dither[index] = dither;
    END_ATOMIC;
}

void pixel_v_load_hsv(
    uint16_t index,
    uint16_t len,
    uint16_t *h,
    uint16_t *s,
    uint16_t *v ){


    uint16_t r, g, b, w;

    // if first pixel and analog mode:
    if( ( index == 0 ) && ( pix_mode == PIX_MODE_ANALOG ) ){

        gfx_v_hsv_to_rgb(
                h[0],
                s[0],
                v[0],
                &r,
                &g,
                &b
            );

        pixel_v_set_analog_rgb( r, g, b );

        return;
    }

    uint16_t transfer_count = len;

    if( ( index + transfer_count ) > MAX_PIXELS ){

        transfer_count = MAX_PIXELS - index;
    }

    // bounds check
    if( ( index + transfer_count ) > MAX_PIXELS ){

        log_v_debug_P( PSTR("pix transfer out of bounds") );
        return;
    }

    if( pix_mode == PIX_MODE_SK6812_RGBW ){

        for( uint16_t i = 0; i < len; i++ ){

            gfx_v_hsv_to_rgbw(
                h[i],
                s[i],
                v[i],
                &r,
                &g,
                &b,
                &w
            );
      
            r /= 256;
            g /= 256;
            b /= 256;
            w /= 256;
            
            // need to do the copy with interrupts disabled,
            // so that way we have access into the arrays without
            // the pixel driver touching them
            ATOMIC;
            array_r[i + index] = r;
            array_g[i + index] = g;
            array_b[i + index] = b;
            array_misc.white[i + index] = w;
            END_ATOMIC;
        }
    }
    else{

        for( uint16_t i = 0; i < len; i++ ){

            gfx_v_hsv_to_rgb(
                h[i],
                s[i],
                v[i],
                &r,
                &g,
                &b
            );
        
            pixel_v_load_rgb16( i + index, r, g, b );
        }
    }

}

void pixel_v_get_rgb_totals( uint16_t *r, uint16_t *g, uint16_t *b ){

    *r = 0;
    *g = 0;
    *b = 0;

    for( uint16_t i = 0; i < gfx_u16_get_pix_count(); i++ ){

        r += array_r[i];
        g += array_g[i];
        b += array_b[i];
    }
}

void pixel_v_clear( void ){

    memset( array_r, 0, sizeof(array_r) );
    memset( array_g, 0, sizeof(array_g) );
    memset( array_b, 0, sizeof(array_b) );
    memset( array_misc.white, 0, sizeof(array_misc.white) );
}

