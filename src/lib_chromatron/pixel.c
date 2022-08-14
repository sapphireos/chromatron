// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
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
#include "pwm.h"

#include "logging.h"
#include "hal_pixel.h"

#ifdef ENABLE_GFX

bool pix_dither;
uint8_t pix_mode;
uint32_t pix_clock;
uint8_t pix_rgb_order;
uint8_t pix_apa102_dimmer = 31;

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

        if( ( pix_mode == PIX_MODE_WS2811 ) ||
            ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

            pix_clock = 800000; // indicate effective bit rate, not actual for 1 wire
        }

        hal_pixel_v_configure();
    }

    return 0;
}

KV_SECTION_META kv_meta_t pixel_info_kv[] = {
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_rgb_order,       pix_i8_kv_handler,    "pix_rgb_order" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_PERSIST, &pix_clock,           pix_i8_kv_handler,    "pix_clock" },
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_PERSIST, &pix_dither,          pix_i8_kv_handler,    "pix_dither" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_mode,            pix_i8_kv_handler,    "pix_mode" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};

void pixel_v_init( void ){

    if( ( pix_mode == PIX_MODE_WS2811 ) ||
        ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

        pix_clock = 800000; // indicate effective bit rate, not actual for 1 wire
    }

    hal_pixel_v_init();
}

void pixel_v_signal( void ){

    thread_v_signal( PIX_SIGNAL_0 );   
}

uint8_t pixel_u8_bytes_per_pixel( uint8_t mode ){

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

#endif