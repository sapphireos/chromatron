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

#ifndef _HAL_PIXEL_H
#define _HAL_PIXEL_H

void hal_pixel_v_init( void );
void hal_pixel_v_configure( void );
bool hal_pixel_b_is_transmitting( void );

void hal_pixel_v_transfer_complete_callback( uint8_t driver ) __attribute__((weak));
void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len );

#endif
