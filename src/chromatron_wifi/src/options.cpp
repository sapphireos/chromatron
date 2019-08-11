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

#include "options.h"

extern "C"{
	#include "user_interface.h"
}

static bool low_power;
static bool led_quiet;
static bool high_speed;
static int8_t midi_channel;

void opt_v_set_low_power( bool mode ){

	low_power = mode;
}

bool opt_b_get_low_power( void ){

	return low_power;
}

void opt_v_set_led_quiet( bool mode ){

	led_quiet = mode;
}

bool opt_b_get_led_quiet( void ){

	return led_quiet;
}

void opt_v_set_high_speed( bool mode ){

	if( mode && !high_speed ){

		system_update_cpu_freq( SYS_CPU_160MHZ );
	}

	high_speed = mode;
}

bool opt_b_get_high_speed( void ){

	return high_speed;
}

void opt_v_set_midi_channel( int8_t channel ){

	midi_channel = channel;
}

int8_t opt_i8_get_midi_channel( void ){

	return midi_channel;
}



