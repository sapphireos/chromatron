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

#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include "target.h"
#include "gfx_lib.h"
#include "wifi_cmd.h"
#include "keyvalue.h"

#define GFX_TIMER           TCD0
#define GFX_TIMER_CCA_vect  TCD0_CCA_vect
#define GFX_TIMER_CCB_vect  TCD0_CCB_vect
#define GFX_TIMER_CCC_vect  TCD0_CCC_vect
#define GFX_TIMER_CCD_vect  TCD0_CCD_vect



void gfx_v_init( void );
bool gfx_b_running( void );
void gfx_v_pixel_bridge_enable( void );
void gfx_v_pixel_bridge_disable( void );

void gfx_v_sync_params( void );

void gfx_v_subscribe_key( catbus_hash_t32 hash, uint8_t tag );
void gfx_v_reset_subscribed( uint8_t tag );

#endif
