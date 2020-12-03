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

#include <stdint.h>
#include "gfx_lib.h"
#include "hsv_to_rgb.h"
#include "keyvalue.h"

#ifdef ENABLE_BG_CAL
static uint16_t green_cal = 65535;
static uint16_t blue_cal = 65535;

KV_SECTION_META kv_meta_t hsv_to_rgb_kv[] = {
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &green_cal,              0,                   "gfx_green_cal" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &blue_cal,               0,                   "gfx_blue_cal" },
};
#endif

void gfx_v_hsv_to_rgb(
    uint16_t h,
    uint16_t s,
    uint16_t v,
    uint16_t *r,
    uint16_t *g,
    uint16_t *b ){

    uint16_t temp_r, temp_g, temp_b, temp_s;
    temp_r = 0;
    temp_g = 0;
    temp_b = 0;

    temp_s = 65535 - s;

    if( h <= 21845 ){
        
        temp_r = ( 21845 - h ) * 3;
        temp_g = 65535 - ( 21845 - h ) * 3;
    }
    else if( h <= 43690 ){
    
        temp_g = ( 43690 - h ) * 3;
        temp_b = 65535 - temp_g;
    }
    else{
    
        temp_b = ( 65535 - h ) * 3;  
        temp_r = 65535 - temp_b;
    }

    // floor saturation
    if( temp_r < temp_s ){

        temp_r = temp_s;
    }

    if( temp_g < temp_s ){

        temp_g = temp_s;
    }

    if( temp_b < temp_s ){

        temp_b = temp_s;
    }

    #ifdef ENABLE_BG_CAL
    // apply cal
    temp_g = ( temp_g * green_cal ) / 65536;
    temp_b = ( temp_b * blue_cal ) / 65536;
    #endif

    // apply brightness
    *r = ( (uint32_t)temp_r * v ) / 65536;
    *g = ( (uint32_t)temp_g * v ) / 65536;
    *b = ( (uint32_t)temp_b * v ) / 65536;
}

void gfx_v_hsv_to_rgbw(
    uint16_t h,
    uint16_t s,
    uint16_t v,
    uint16_t *r,
    uint16_t *g,
    uint16_t *b,
    uint16_t *w ){

    uint16_t temp_r, temp_g, temp_b, temp_s;
    temp_r = 0;
    temp_g = 0;
    temp_b = 0;

    temp_s = 65535 - s;

    if( h <= 21845 ){
    
        temp_r = ( 21845 - h ) * 3;
        temp_g = 65535 - ( 21845 - h ) * 3;

        // apply saturation to RGB
        temp_r = ( (uint32_t)temp_r * s ) / 65536;
        temp_g = ( (uint32_t)temp_g * s ) / 65536;
    }
    else if( h <= 43690 ){
    
        temp_g = ( 43690 - h ) * 3;
        temp_b = 65535 - temp_g;

        // apply saturation to RGB
        temp_g = ( (uint32_t)temp_g * s ) / 65536;
        temp_b = ( (uint32_t)temp_b * s ) / 65536;
    }
    else{
    
        temp_b = ( 65535 - h ) * 3;  
        temp_r = 65535 - temp_b;

        // apply saturation to RGB
        temp_r = ( (uint32_t)temp_r * s ) / 65536;
        temp_b = ( (uint32_t)temp_b * s ) / 65536;
    }

    #ifdef ENABLE_BG_CAL
    // apply cal
    temp_g = ( temp_g * green_cal ) / 65536;
    temp_b = ( temp_b * blue_cal ) / 65536;
    #endif

    // apply brightness
    *r = ( (uint32_t)temp_r * v ) / 65536;
    *g = ( (uint32_t)temp_g * v ) / 65536;
    *b = ( (uint32_t)temp_b * v ) / 65536;
    *w = ( (uint32_t)temp_s * v ) / 65536;
}


// #include "hue_table.txt"

// void gfx_v_hsv_to_rgbw8(
//     uint16_t h,
//     uint16_t s,
//     uint16_t v,
//     uint8_t *r,
//     uint8_t *g,
//     uint8_t *b,
//     uint8_t *w ){

//     uint8_t temp_r, temp_g, temp_b, temp_s;

//     temp_s = 255 - ( s >> 8 );

//     temp_r = hue_table[h][0];
//     temp_g = hue_table[h][1];
//     temp_b = hue_table[h][2];
        
//     // apply brightness
//     *r = ( (uint32_t)temp_r * v ) / 65536;
//     *g = ( (uint32_t)temp_g * v ) / 65536;
//     *b = ( (uint32_t)temp_b * v ) / 65536;
//     *w = ( (uint32_t)temp_s * v ) / 65536;
// }

