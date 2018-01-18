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

#ifndef _SERVER_H_
#define _SERVER_H_

#include "gfx_lib.h"

#define CHROMATRON_SERVER_PORT          8004

#define CHROMA_SERVER_TIMEOUT           8

#define CHROMA_MSG_TYPE_HSV                1
#define CHROMA_MSG_TYPE_RGB                2
typedef struct{
    uint8_t type;
    uint8_t flags;
    uint16_t index; // pixel index (NOT byte index)
    uint8_t count; // pixel count (NOT byte count)
    uint16_t data0;
} chroma_msg_pixel_t;

// 16 bits, 3 channels, fitting in 512 bytes or less
#define CHROMA_SVR_MAX_PIXELS ( 80 )



void svr_v_init( void );


#endif
