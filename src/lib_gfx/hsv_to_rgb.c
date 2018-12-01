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

#include <stdint.h>
#include "hsv_to_rgb.h"

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

    // if( h <= 32767 ){ // h <= 0.5

    //     if( h <= 10922 ){       // 0.0 <= h <= 0.1667

    //         temp_r = 65535;
    //         temp_g = h * 6;
    //         temp_b = 0;
    //     }
    //     else if( h <= 21845 ){  // 0.1667 < h <= 0.333

    //         temp_r = ( 21845 - h ) * 6;
    //         temp_g = 65535;
    //         temp_b = 0;
    //     }
    //     else{                   // 0.333 > h <= 0.5

    //         temp_r = 0;
    //         temp_g = 65535;
    //         temp_b = ( h - 21845 ) * 6;
    //     }
    // }
    // else{ // h > 0.5

    //     if( h <= 43690 ){       // 0.5 < h <= 0.6667

    //         temp_r = 0;
    //         temp_g = ( 43690 - h ) * 6;
    //         temp_b = 65535;
    //     }
    //     else if( h <= 54612 ){  // 0.6667 < h <= 0.8333

    //         temp_r = ( h - 43690 ) * 6;
    //         temp_g = 0;
    //         temp_b = 65535;
    //     }
    //     else{                   // 0.8333 < h <= 0.9999

    //         temp_r = 65535;
    //         temp_g = 0;
    //         temp_b = ( 65535 - h ) * 6;
    //     }

    // }

    if( h <= 21845 ){
    
        temp_r = ( 21845 - h ) * 3;
        temp_g = 65535 - temp_r;
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
        temp_g = 65535 - temp_r;

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

    // apply brightness
    *r = ( (uint32_t)temp_r * v ) / 65536;
    *g = ( (uint32_t)temp_g * v ) / 65536;
    *b = ( (uint32_t)temp_b * v ) / 65536;
    *w = ( (uint32_t)temp_s * v ) / 65536;
}


#include "hue_table.txt"

void gfx_v_hsv_to_rgbw8(
    uint16_t h,
    uint16_t s,
    uint16_t v,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *w ){

    uint8_t temp_r, temp_g, temp_b, temp_s;

    temp_s = 255 - ( s >> 8 );

    *r = hue_table[h][0];
    *g = hue_table[h][1];
    *b = hue_table[h][2];
        
    // apply brightness
    *r = ( (uint32_t)temp_r * v ) / 65536;
    *g = ( (uint32_t)temp_g * v ) / 65536;
    *b = ( (uint32_t)temp_b * v ) / 65536;
    *w = ( (uint32_t)temp_s * v ) / 65536;
}

