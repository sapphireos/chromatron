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

// #define ADC_CHANNEL_VSUPPLY     3 // divided VIN measurement

// #define ADC_CHANNEL_EXT         12

// #define ADC_CHANNEL_BG          13 // ADC bandgap
#define ADC_CHANNEL_VCC         14 // VCC

// note the internal temp sensor on the XMEGA A4 is useless
// without a room temperature calibration
// #define ADC_CHANNEL_TEMP        15 // internal temperature sensor

// #define ADC_VREF_INT1V          ADC_REFSEL_INT1V_gc
// #define ADC_VREF_INTVCC_DIV1V6  ADC_REFSEL_INTVCC_gc // VCC / 1.6V (approx 2.0V at 3.3VCC)

// channels - on board
#define ANALOG_CHANNEL_ADC0     IO_PIN_4_ADC0
#define ANALOG_CHANNEL_ADC1     IO_PIN_5_ADC1
#define ANALOG_CHANNEL_DAC0     IO_PIN_6_DAC0 // DAC channels are also ADC inputs
#define ANALOG_CHANNEL_DAC1     IO_PIN_7_DAC1

// vsupply pin
// #define ADC_VSUPPLY_PORT        PORTA
// #define ADC_VSUPPLY_PINCTRL     PIN3CTRL
// #define ADC_VSUPPLY_PIN         3

// #define ADC_OFFSET_SAMPLES      64


// channels
#define ADC_CHANNEL_0           0
#define ADC_CHANNEL_1           1
#define ADC_CHANNEL_2           2
#define ADC_CHANNEL_3           3
#define ADC_CHANNEL_4           4
#define ADC_CHANNEL_5           5
#define ADC_CHANNEL_6           6
#define ADC_CHANNEL_7           7
#define ADC_CHANNEL_1V2_BG      8 // 1.2 volt bandgap
#define ADC_CHANNEL_0V0_AVSS    9 // 0.0 volt AVSS
#define ADC_CHANNEL_TEMP        10 // internal temperature sensor
#define ADC_CHANNEL_VSUPPLY     ( ADC_CHANNEL_0 ) // voltage monitor (connected to channel 0)


// reference selections
#define ADC_REF_AREF    0
#define ADC_REF_AVDD    1   // 1.8 volt internal supply
#define ADC_REF_1V5     2   // 1.5 volt reference
#define ADC_REF_1V6     3   // 1.6 volt reference

#endif
