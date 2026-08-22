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

#ifndef _AMG8833_H_
#define _AMG8833_H_

#define AMG8833_PIXEL_COUNT                 64
#define AMG8833_X_DIM                       8
#define AMG8833_Y_DIM                       8

#define AMG8833_I2C_ADDR_0                  0x68 // AD_SELECT = 0, 0x69 if AD_SELECT = 1
#define AMG8833_I2C_ADDR_1                  0x69

#define AMG8833_REG_PCTL                    0x00
#define AMG8833_PCTL_MODE_NORMAL            0x00
#define AMG8833_PCTL_MODE_SLEEP             0x10
#define AMG8833_PCTL_MODE_STANDBY_60SEC     0x20
#define AMG8833_PCTL_MODE_STANDBY_10SEC     0x21

#define AMG8833_REG_RST                     0x01
#define AMG8833_RST_FLAG_RESET              0x30
#define AMG8833_RST_INIT_RESET              0x3F

#define AMG8833_REG_FPSC                    0x02
#define AMG8833_FPSC_1FPS                   0x01
#define AMG8833_FPSC_10FPS                  0x00

#define AMG8833_REG_INTC                    0x03
#define AMG8833_INTC_INTMOD                 0x02
#define AMG8833_INTC_INTEN                  0x01

#define AMG8833_REG_STAT                    0x04
#define AMG8833_STAT_OVF_THS                0x08
#define AMG8833_STAT_OVF_IRS                0x04
#define AMG8833_STAT_INTF                   0x02

#define AMG8833_REG_SCLR                    0x05
#define AMG8833_SCLR_OVT_CLR                0x08
#define AMG8833_SCLR_OVS_CLR                0x04
#define AMG8833_SCLR_INT_CLR                0x02

#define AMG8833_REG_AVE                     0x07
#define AMG8833_REG_INTHL                   0x08
#define AMG8833_REG_INTHH                   0x09
#define AMG8833_REG_INTLL                   0x0A
#define AMG8833_REG_INTLH                   0x0B
#define AMG8833_REG_IHYSL                   0x0C
#define AMG8833_REG_IHYSH                   0x0D
#define AMG8833_REG_TTHL                    0x0E
#define AMG8833_REG_TTHH                    0x0F

#define AMG8833_REG_PIXEL_START             0x80



void amg8833_v_init( void );
void amg8833_v_reset( void );
void amg8833_v_set_mode( uint8_t mode );
void amg8833_v_set_rate( uint8_t rate );
int16_t amg8833_i16_read_thermistor( void );
int16_t amg8833_i16_read_pixel( uint8_t index );
void amg8833_v_read_pixels( int16_t pixels[AMG8833_PIXEL_COUNT] );


#endif