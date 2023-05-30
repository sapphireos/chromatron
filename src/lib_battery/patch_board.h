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

#ifndef _PATCH_BOARD_H
#define _PATCH_BOARD_H

// PCA9536 connections on Patch Board
// #define PATCH_PCA9536_IO_SOLAR_EN   0
// #define PATCH_PCA9536_IO_DC_DETECT  1
// #define PATCH_PCA9536_IO_IO2        2
// #define PATCH_PCA9536_IO_MOTOR_IN_2 3

// #define PATCH_ADC_CH_TILT			0
// #define PATCH_ADC_CH_SOLAR_VOLTS	1

// #define PATCH_ADC_VREF              3300


void patchboard_v_init( void );

bool patchboard_b_read_dc_detect( void );
// bool patchboard_b_read_io2( void );

void patchboard_v_set_solar_en( bool enable );
// void patchboard_v_set_motor2( bool enable );

// uint16_t patchboard_u16_read_tilt_volts( void );
uint16_t patchboard_u16_read_solar_volts( void );

#endif
