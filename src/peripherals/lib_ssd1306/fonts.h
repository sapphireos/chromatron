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

#ifndef __FONTS_H_
#define __FONTS_H_

#include "gfxfont.h"

typedef struct{
    uint8_t *bitmap;
    uint8_t width_px;
    uint8_t height_px;
    uint8_t x_advance;
    uint8_t y_advance;
    int8_t x_offset;
    int8_t y_offset;
} glyph_info_t;


glyph_info_t fonts_g_get_glyph( uint8_t _font, uint8_t c );


#endif