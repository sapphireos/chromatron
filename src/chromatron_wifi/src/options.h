// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "bool.h"

void opt_v_set_low_power( bool mode );
bool opt_b_get_low_power( void );
void opt_v_set_led_quiet( bool mode );
bool opt_b_get_led_quiet( void );
void opt_v_set_high_speed( bool mode );
bool opt_b_get_high_speed( void );
void opt_v_set_midi_channel( int8_t channel );
int8_t opt_i8_get_midi_channel( void );

#endif
