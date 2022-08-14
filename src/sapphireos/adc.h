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


#ifndef _ADC_H
#define _ADC_H

#include "hal_adc.h"

void adc_v_init( void );
void adc_v_shutdown( void );

void adc_v_set_ref( uint8_t ref );

uint16_t adc_u16_read_raw( uint8_t channel );
uint16_t adc_u16_read_vcc( void );

float adc_f_read( uint8_t channel );
uint16_t adc_u16_read_mv( uint8_t channel );

uint16_t adc_u16_read_supply_voltage( void );

uint16_t adc_u16_convert_to_millivolts( uint16_t raw_value );

#endif
