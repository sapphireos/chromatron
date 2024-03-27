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

#ifdef ENABLE_SOLAR

#define SOLAR_CONTROL_POLLING_RATE			100
#define SOLAR_SENSOR_POLLING_RATE			250
#define SOLAR_CYCLE_POLLING_RATE			1000

#define SOLAR_DC_FILTER_DEPTH				4
#define SOLAR_VOLTS_FILTER_DEPTH			32

#define SOLAR_MIN_CHARGE_VOLTS				5500
#define SOLAR_MIN_CHARGE_LIGHT_DEFAULT		500000


#define SOLAR_MODE_DISCHARGE			0
#define SOLAR_MODE_CHARGE_DC			1
#define SOLAR_MODE_CHARGE_SOLAR			2
#define SOLAR_MODE_FULL_CHARGE			3
#define SOLAR_MODE_STOPPED				4
#define SOLAR_MODE_SHUTDOWN	  			5
#define SOLAR_MODE_FAULT	  			6


#define SOLAR_CYCLE_UNKNOWN				0
#define SOLAR_CYCLE_DAY					1
#define SOLAR_CYCLE_DUSK				2
#define SOLAR_CYCLE_TWILIGHT			3
#define SOLAR_CYCLE_NIGHT				4
#define SOLAR_CYCLE_DAWN				5


#define SOLAR_CYCLE_VALIDITY_THRESH		30

void solar_v_init( void );

// bool solar_b_has_patch_board( void );

// bool solar_b_has_charger2_board( void );

uint8_t solar_u8_get_state( void );
bool solar_b_is_charging( void );

// bool solar_b_is_dc_power( void );
// bool solar_b_is_solar_power( void );

#endif

#endif
