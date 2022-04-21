/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifndef _ADS1015_H_
#define _ADS1015_H_

#include "sapphire.h"

#define ADS1015_N_CHANNELS		4

#define ADS1015_I2C_ADDR0       0x48
#define ADS1015_I2C_ADDR1       0x49
#define ADS1015_I2C_ADDR2       0x4A
#define ADS1015_I2C_ADDR3       0x4B

#define ADS1015_GAIN_4096           4
#define ADS1015_GAIN_2048           3
#define ADS1015_GAIN_1024           2
#define ADS1015_GAIN_512            1
#define ADS1015_GAIN_256            0

#define ADS1015_REG_CONV        0x00
#define ADS1015_REG_CFG         0x01
#define ADS1015_REG_LO_THRESH   0x02
#define ADS1015_REG_HI_THRESH   0x03

#define ADS1015_OS_CONV_START       0x8000
#define ADS1015_CONV_DONE    		0x8000
#define ADS1015_MUX_AIN0            0x4000
#define ADS1015_MUX_AIN1            0x5000
#define ADS1015_MUX_AIN2            0x6000
#define ADS1015_MUX_AIN3            0x7000
#define ADS1015_CFG_GAIN_4096       0x0200
#define ADS1015_CFG_GAIN_2048       0x0400
#define ADS1015_CFG_GAIN_1024       0x0600
#define ADS1015_CFG_GAIN_512        0x0800
#define ADS1015_CFG_GAIN_256        0x0A00
#define ADS1015_MODE_SINGLE_SHOT    0x0100
#define ADS1015_MODE_CONT           0x0000
#define ADS1015_RATE_128            0x0000
#define ADS1015_RATE_1600           0x0080
#define ADS1015_RATE_3300           0x00D0


int8_t ads1015_i8_init( uint8_t device_addr );

void ads1015_v_start( uint8_t device_addr, uint8_t channel, uint8_t gain_setting );
bool ads1015_b_poll( uint8_t device_addr, uint8_t channel, uint8_t gain_setting, int32_t *microvolts );

int32_t ads1015_i32_read( uint8_t device_addr, uint8_t channel, uint8_t gain_setting );
int32_t ads1015_i32_read_filtered( uint8_t device_addr, uint8_t channel, uint8_t samples, uint8_t gain_setting );

#endif