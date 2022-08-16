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

#ifndef _MPU9250_H_
#define _MPU9250_H_

#define MPU9250_ACCEL_SCALE_2G				0
#define MPU9250_ACCEL_SCALE_4G				1
#define MPU9250_ACCEL_SCALE_8G				2
#define MPU9250_ACCEL_SCALE_16G				3

#define MPU9250_I2C_ADDR_0                  0x68 // AD0 = 0, 0x69 if AD0 = 1
#define MPU9250_I2C_ADDR_1                  0x69

#define MPU9250_REG_ACCEL_CONFIG			28
#define MPU9250_BIT_X_ACCEL_SELF_TEST 		0x80
#define MPU9250_BIT_Y_ACCEL_SELF_TEST 		0x40
#define MPU9250_BIT_Z_ACCEL_SELF_TEST 		0x20
#define MPU9250_BIT_ACCEL_SCALE_MASK     	0x18
#define MPU9250_BIT_ACCEL_SCALE_SHIFT    	2

#define MPU9250_REG_ACCEL_CONFIG2			29

#define MPU9250_REG_ACCEL_XOUT_H			59
#define MPU9250_REG_ACCEL_XOUT_L			60
#define MPU9250_REG_ACCEL_YOUT_H			61
#define MPU9250_REG_ACCEL_YOUT_L			62
#define MPU9250_REG_ACCEL_ZOUT_H			63
#define MPU9250_REG_ACCEL_ZOUT_L			64

#define MPU9250_REG_WHO_AM_I	    		117
#define MPU9250_VAL_WHO_AM_I	    		0x71



void mpu9250_v_init( void );
void mpu9250_v_set_accel_scale( uint8_t scale );

int16_t mpu9250_i16_read_accel_x( void );
int16_t mpu9250_i16_read_accel_y( void );
int16_t mpu9250_i16_read_accel_z( void );

#endif


