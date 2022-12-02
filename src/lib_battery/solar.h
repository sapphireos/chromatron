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

#include "adc.h"

#define SOLAR_MOTOR_RATE				100

#define SOLAR_MOTOR_MOVE_TIMEOUT		2000
#define SOLAR_MOTOR_LOCK_TIMEOUT		10000

#define SOLAR_TILT_MOVEMENT_THRESHOLD 	3 // degrees

#define SOLAR_ANGLE_MIN					0 // degrees
#define SOLAR_ANGLE_MAX					60 // degrees


#define SOLAR_TILT_SENSOR_IO 	IO_PIN_34_A2
#define SOLAR_TILT_MOTOR_IO_0 	IO_PIN_16_RX
#define SOLAR_TILT_MOTOR_IO_1 	IO_PIN_17_TX

#define SOLAR_TILT_ADC_SAMPLES      8
#define SOLAR_TILT_FILTER	        4

void solar_v_init( void );

uint8_t solar_u8_get_tilt_angle( void );
uint8_t solar_u8_get_target_angle( void );
void solar_v_set_tilt_angle( uint8_t angle );

#endif
