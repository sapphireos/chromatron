/*
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
*/

#ifndef _SOLAR_H
#define _SOLAR_H

#include "sapphire.h"

#define SOLAR_MIN_CHARGE_VOLTS				4800
#define SOLAR_MIN_CHARGE_LIGHT_DEFAULT		500000

void solar_v_init( void );

bool solar_b_has_patch_board( void );
bool solar_b_has_charger2_board( void );

// bool solar_b_is_dc_power( void );
// bool solar_b_is_solar_power( void );

#endif
