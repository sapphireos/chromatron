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

#ifndef _PCA9685_H_
#define _PCA9685_H_


// TODO ADDRESSES ARE WRONG!
#define PCA9685_I2C_ADDR_0                  0x40 // AD0 = 0, 0x69 if AD0 = 1
#define PCA9685_I2C_ADDR_1                  0x41

#define PCA9685_N_CHANNELS 			16
#define PCA9685_MAX_PWM				4095

#define PCA9685_OSC_FREQ 			25000000
#define PCA9685_MIN_PRESCALE 		3
#define PCA9685_MAX_PRESCALE 		255

// registers
#define PCA9685_REG_MODE1 			0x00
#define PCA9685_REG_MODE2 			0x01
#define PCA9685_REG_PRESCALE		0xfe

#define PCA9685_MODE1_BIT_AI 		(1<<5)
#define PCA9685_MODE1_BIT_SLEEP		(1<<4)


// only need the first register offsets
#define PCA9685_REG_LED0_ON_L		0x06
#define PCA9685_REG_LED0_ON_H		0x07
#define PCA9685_REG_LED0_OFF_L		0x08
#define PCA9685_REG_LED0_OFF_H		0x09


void pca9685_v_set_freq( uint8_t prescale );
uint32_t pca9685_u32_get_freq( void );
void pca9685_v_set( uint8_t channel, uint16_t value );
void pca9685_v_fade( uint8_t channel, uint16_t value, uint16_t fade_time );

void pca9685_v_init( uint8_t addr );


#endif


