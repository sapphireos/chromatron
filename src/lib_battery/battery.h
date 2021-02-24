/*
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
*/

#ifndef _BATTERY_H
#define _BATTERY_H

#include "sapphire.h"

#if defined(ESP8266)
#define UI_BUTTON   IO_PIN_6_DAC0
#elif defined(ESP32)
#define UI_BUTTON   IO_PIN_21
#endif

// PCA9536 connections on Charger2
#define BATT_IO_QON     0
#define BATT_IO_S2      1
#define BATT_IO_SPARE   2
#define BATT_IO_BOOST   3


void batt_v_init( void );

#endif
