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

#ifndef _MCP73831_H_
#define _MCP73831_H_

#include "target.h"
#include "hal_io.h"


#define MCP73831_FLOAT_VOLTAGE 4200

#ifdef ESP32
    #define MCP73831_IO_MCU_PWR     IO_PIN_25_A1
    #define MCP73831_IO_PIXEL       IO_PIN_16_RX
    #define MCP73831_IO_VBUS_MON    IO_PIN_34_A2
#elif defined(BLUE_SAPPHIRE)

    // placeholders!
    #define MCP73831_IO_MCU_PWR     IO_PIN_ADC0
    #define MCP73831_IO_PIXEL       IO_PIN_ADC1
    #define MCP73831_IO_VBUS_MON    IO_PIN_ADC2
#else

#endif


void mcp73831_v_init( void );

void mcp73831_v_shutdown( void );

void mcp73831_v_enable_pixels( void );
void mcp73831_v_disable_pixels( void );

uint16_t mcp73831_u16_get_batt_volts( void );
uint16_t mcp73831_u16_get_vbus_volts( void );
bool mcp73831_b_is_charging( void );


#endif

