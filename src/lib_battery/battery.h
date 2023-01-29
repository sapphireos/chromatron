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
#define BATT_EMERGENCY_VOLTAGE		3050

#define BATT_RECHARGE_THRESHOLD		100

#define BATT_MIN_CHARGE_VBUS_VOLTS				4000

#define BATT_PIX_COUNT_LOW_POWER_THRESHOLD		100

void batt_v_init( void );
bool batt_b_enabled( void );

uint16_t batt_u16_get_charge_voltage( void );
uint16_t batt_u16_get_min_discharge_voltage( void );

bool batt_b_is_mcp73831_enabled( void );
void batt_v_enable_charge( void );
void batt_v_disable_charge( void );
int8_t batt_i8_get_batt_temp( void );
uint16_t batt_u16_get_vbus_volts( void );
bool batt_b_is_vbus_connected( void );
uint16_t batt_u16_get_batt_volts( void );
uint16_t batt_u16_get_charge_current( void );
uint8_t batt_u8_get_soc( void );
bool batt_b_is_charging( void );
bool batt_b_is_charge_complete( void );
bool batt_b_is_external_power( void );
bool batt_b_is_batt_fault( void );
uint16_t batt_u16_get_nameplate_capacity( void );

void batt_v_shutdown_power( void );

#endif
