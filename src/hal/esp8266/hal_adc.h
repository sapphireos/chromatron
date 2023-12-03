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

#ifdef ENABLE_COPROCESSOR

#define ANALOG_CHANNEL_ADC0     IO_PIN_4_ADC0
#define ANALOG_CHANNEL_ADC1     IO_PIN_5_ADC1
#define ANALOG_CHANNEL_DAC0     IO_PIN_6_DAC0
#define ANALOG_CHANNEL_DAC1     IO_PIN_7_DAC1

#define ADC_CHANNEL_VCC         14 // VCC
#define ADC_CHANNEL_VSUPPLY     3 // divided VIN measurement

// note the internal temp sensor on the XMEGA A4 is useless
// without a room temperature calibration
#define ADC_CHANNEL_TEMP        15 // internal temperature sensor

#endif

// #define ADC_CHANNEL_VSUPPLY     (IO_PIN_ANALOG_COUNT) 	   // divided VIN measurement
// #define ADC_CHANNEL_REF         (IO_PIN_ANALOG_COUNT + 1) // ADC reference


#endif
