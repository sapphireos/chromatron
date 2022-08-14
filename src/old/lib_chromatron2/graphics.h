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

#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include "target.h"
#include "gfx_lib.h"
#include "wifi_cmd.h"
#include "keyvalue.h"


void gfx_v_init( void );
bool gfx_b_running( void );
uint16_t gfx_u16_get_frame_number( void );
uint32_t gfx_u32_get_frame_ts( void );
void gfx_v_set_frame_number( uint16_t frame );
void gfx_v_set_sync0( uint16_t frame, uint32_t ts );

#endif
