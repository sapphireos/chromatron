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

#include "fonts.h"

#include "font_cozette.h"


/*

Font bitmap data bytes are each column


*/

static const GFXfont* fonts[] __attribute__ ((used)) = {
    &CozetteVector6pt7b,
};

static const GFXfont* get_font( uint8_t _font ){

    if( _font >= cnt_of_array(fonts) ){

        _font = 0;
    }

    return fonts[_font];
}

glyph_info_t fonts_g_get_glyph( uint8_t _font, uint8_t c ){

    const GFXfont *font = get_font( _font );

    if( ( c < font->first ) || ( c > font->last ) ){

        c = '.';
    }

    c -= font->first;

    glyph_info_t info = {
        .bitmap     = &font->bitmap[font->glyph[c].bitmapOffset],
        .width_px   = font->glyph[c].width,
        .height_px  = font->glyph[c].height,
        .x_advance  = font->glyph[c].xAdvance,
        .y_advance  = font->yAdvance,
        .x_offset   = font->glyph[c].xOffset,
        .y_offset   = font->glyph[c].yOffset,
    };

    return info;
}
