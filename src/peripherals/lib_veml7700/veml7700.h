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

#ifndef _VEML7700_H_
#define _VEML7700_H_

#define VEML7700_I2C_ADDR 0x10

#define VEML7700_REG_ALS_CONF_0     0
#define VEML7700_REG_ALS_WH         1
#define VEML7700_REG_ALS_WL         2
#define VEML7700_REG_PSM            3
#define VEML7700_REG_ALS            4
#define VEML7700_REG_WHITE          5
#define VEML7700_REG_ALS_INT        6

// shutdown bit: (in CONF 0)
#define VEML7700_BIT_ALS_SD			( 1 << 0 )

// PSM modes;
#define VEML7700_PSM_MODE_MASK		0b0110
#define VEML7700_PSM_EN_MASK		0b0001

#define VEML7700_ALS_GAIN_SHIFT      11
#define VEML7700_ALS_GAIN_MASK       0x03
#define VEML7700_ALS_GAIN_x1         0
#define VEML7700_ALS_GAIN_x2         1
#define VEML7700_ALS_GAIN_x0_125     2
#define VEML7700_ALS_GAIN_x0_25      3

#define VEML7700_ALS_INT_TIME_SHIFT  6
#define VEML7700_ALS_INT_TIME_MASK   0x0F
#define VEML7700_ALS_INT_TIME_25ms   0b1100
#define VEML7700_ALS_INT_TIME_50ms   0b1000
#define VEML7700_ALS_INT_TIME_100ms  0b0000
#define VEML7700_ALS_INT_TIME_200ms  0b0001
#define VEML7700_ALS_INT_TIME_400ms  0b0010
#define VEML7700_ALS_INT_TIME_800ms  0b0011

void veml7700_v_init( void );

void veml7700_v_configure( uint8_t _gain, uint8_t _int_time );
uint16_t veml7700_u16_read_als( void );
uint16_t veml7700_u16_read_white( void );


#endif
