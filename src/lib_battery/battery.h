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

#ifndef _BATTERY_H
#define _BATTERY_H

#include "sapphire.h"

#define BATT_MAX_FLOAT_VOLTAGE		4150
#define BATT_CUTOFF_VOLTAGE			3100

#define BATT_RECHARGE_THRESHOLD		100


#define BATT_WALL_POWER_THRESHOLD	3800 // mv

#define BATT_PIX_COUNT_LOW_POWER_THRESHOLD		100

// PCA9536 connections on Charger2
// #define BATT_IO_QON     0
// #define BATT_IO_S2      1
// #define BATT_IO_SPARE   2
// #define BATT_IO_BOOST   3


void batt_v_init( void );


uint16_t batt_u16_get_charge_voltage( void );
uint16_t batt_u16_get_discharge_voltage( void );

// APIs to control pixel strip power switch
// void batt_v_enable_pixels( void );
// void batt_v_disable_pixels( void );

// bool batt_b_pixels_enabled( void );

bool batt_b_is_mcp73831_enabled( void );
void batt_v_enable_charge( void );
void batt_v_disable_charge( void );
int8_t batt_i8_get_batt_temp( void );
uint16_t batt_u16_get_vbus_volts( void );
uint16_t batt_u16_get_batt_volts( void );
uint8_t batt_u8_get_soc( void );
bool batt_b_is_charging( void );
bool batt_b_is_charge_complete( void );
bool batt_b_is_wall_power( void );
bool batt_b_is_batt_fault( void );
uint16_t batt_u16_get_nameplate_capacity( void );

// bool batt_b_is_button_pressed( uint8_t button );
// bool batt_b_is_button_hold( uint8_t button );
// bool batt_b_is_button_released( uint8_t button );
// bool batt_b_is_button_hold_released( uint8_t button );

#endif
