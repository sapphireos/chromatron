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

#ifndef __PCA9536_H
#define __PCA9536_H

#define PCA9536_I2C_ADDR        0x41

#define PCA9536_REG_INPUT       0x00
#define PCA9536_REG_OUTPUT      0x01
#define PCA9536_REG_INVERT      0x02
#define PCA9536_REG_CONFIG      0x03

#define PCA9536_IO_COUNT        4

int8_t pca9536_i8_init( void );

void pca9536_v_set_output( uint8_t ch );
void pca9536_v_set_input( uint8_t ch );

void pca9536_v_gpio_write( uint8_t ch, uint8_t state );
bool pca9536_b_gpio_read( uint8_t ch );

#endif
