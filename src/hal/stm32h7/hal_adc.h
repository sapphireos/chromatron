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


#ifndef _HAL_ADC_H
#define _HAL_ADC_H

#include "hal_io.h"

#define ADC_CHANNEL_VSUPPLY     (IO_PIN_ANALOG_COUNT) 	  // divided VIN measurement
#define ADC_CHANNEL_REF         (IO_PIN_ANALOG_COUNT + 1) // ADC reference
#define ADC_CHANNEL_VBAT        (IO_PIN_ANALOG_COUNT + 2) // VBAT / 4


#endif
