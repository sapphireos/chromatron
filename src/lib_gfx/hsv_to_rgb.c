// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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
#include "hsv_to_rgb.h"
#include "keyvalue.h"

#ifdef ENABLE_GFX

// #define HSV_DEBUG

static bool max_power_sat;

KV_SECTION_META kv_meta_t max_power_sat_kv[] = {
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &max_power_sat,        0,                   "gfx_enable_max_power_sat" },
};

#ifndef GEN_HUE_CURVE

#ifdef ENABLE_BG_CAL

static uint16_t green_cal = 65535;
static uint16_t blue_cal = 65535;

KV_SECTION_META kv_meta_t bg_cal_kv[] = {
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &green_cal,              0,                   "gfx_green_cal" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &blue_cal,               0,                   "gfx_blue_cal" },
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &max_power_white,        0,                   "gfx_enable_max_power_white" },
};
#endif

#ifdef HSV_DEBUG
static uint16_t hsv_debug_hue;
static uint16_t hsv_debug_r;
static uint16_t hsv_debug_g;
static uint16_t hsv_debug_b;

KV_SECTION_META kv_meta_t hsv_to_rgb_kv[] = {
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &hsv_debug_hue,              0,                   "hsv_debug_hue" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &hsv_debug_r,                0,                   "hsv_debug_r" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &hsv_debug_g,                0,                   "hsv_debug_g" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &hsv_debug_b,                0,                   "hsv_debug_b" },
};
#endif

#endif

#define HUE_CURVE_ADVANCED

static inline void hsv_to_rgb_core(
    uint16_t h,
    uint16_t *r,
    uint16_t *g,
    uint16_t *b ){

    #ifndef HUE_CURVE_ADVANCED
    if( h <= 21845 ){
        
        *r = ( 21845 - h ) * 3;
        *g = 65535 - ( 21845 - h ) * 3;
    }
    else if( h <= 43690 ){
    
        *g = ( 43690 - h ) * 3;
        *b = 65535 - *g;
    }
    else{
    
        *b = ( 65535 - h ) * 3;  
        *r = 65535 - *b;
    }

    #else

    #define MAP(h, h1, h2, v1, v2) ( ( ( (int32_t)v2 - (int32_t)v1 ) * ( h - h1 ) ) / ( h2 - h1 ) + v1 )
    
    if( h <= 8191 ){        // red to orange

        *r = MAP( h, 0,     8191,   65535, 43690 );
        *g = MAP( h, 0,     8191,   0,     21845 );
    }
    else if( h <= 16383 ){  // orange to yellow

        *r = MAP( h, 8191,  16383,  43690, 43690 );
        *g = MAP( h, 8191,  16383,  21845, 43690 );   
    }
    else if( h <= 24575 ){  // yellow to green

        *r = MAP( h, 16383, 24575,  43690, 0 );
        *g = MAP( h, 16383, 24575,  43690, 65535 );   
    }
    else if( h <= 32767 ){  // green to cyan

        *g = MAP( h, 24575, 32767,  65535, 43690 );   
        *b = MAP( h, 24575, 32767,  0,     21845 );   
    }
    else if( h <= 40689 ){  // cyan to blue

        *g = MAP( h, 32767, 40690,  43690, 0 );   
        *b = MAP( h, 32767, 40690,  21845, 65535 );   
    }
    else if( h <= 49151 ){  // blue to purple

        *b = MAP( h, 40690, 49151,  65535, 43690 );   
        *r = MAP( h, 40690, 49151,  0,     21845 );   
    }
    else if( h <= 57343 ){  // purple to magenta

        *b = MAP( h, 49151, 57343,  43690, 21845 );   
        *r = MAP( h, 49151, 57343,  21845, 43690 );      
    }
    else{                   // magenta to red

        *b = MAP( h, 57343, 65535,  21845, 0 );   
        *r = MAP( h, 57343, 65535,  43690, 65535 );      
    }

    #endif

    #ifdef HSV_DEBUG
    hsv_debug_hue = h;
    hsv_debug_r = *r;
    hsv_debug_g = *g;
    hsv_debug_b = *b;
    #endif
}

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

    hsv_to_rgb_core( h, &temp_r, &temp_g, &temp_b );

    temp_s = 65535 - s;

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
    
    hsv_to_rgb_core( h, &temp_r, &temp_g, &temp_b );

    // apply saturation to RGB

    // if max power sat is enabled, there will be no RGB attenuation
    if( !max_power_sat ){

        // attenuate RGB channels with increasing saturation
        temp_r = ( (uint32_t)temp_r * s ) / 65536;
        temp_g = ( (uint32_t)temp_g * s ) / 65536;
        temp_b = ( (uint32_t)temp_b * s ) / 65536;
    }
    else{

        // max power sat mode

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

#endif

#ifdef GEN_HUE_CURVE

#include <stdio.h>

int main( void ){

    printf( "Generating hue curve...\n" );

    uint16_t r, g, b;

    FILE* f = fopen("hue_curve.csv", "w" );

    fprintf( f, "hue, red, green, blue\n" );

    for( uint16_t h = 0; h < 65535; h++ ){

        r = 0;
        g = 0;
        b = 0;

        hsv_to_rgb_core( h, &r, &g, &b );

        fprintf( f, "%d, %d, %d, %d\n", h, r, g, b );
    }

    fclose( f );

    printf( "Done!\n" );

    return 0;
}

#endif
