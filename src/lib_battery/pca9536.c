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

#include "sapphire.h"

#include "pca9536.h"

// PCA9536 I2C GPIO driver

int8_t pca9536_i8_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    // write test pattern and then check
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_INVERT, 0x0f );
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_INVERT, 0x06 );

    if( i2c_u8_read_reg8( PCA9536_I2C_ADDR, PCA9536_REG_INVERT ) != 0x06 ){

        return -1;
    }

    // reset inversion
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_INVERT, 0x00 );

    // set all bits to input
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_CONFIG, 0xff );

    // set all low
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_OUTPUT, 0xf0 );

    return 0;
}

void pca9536_v_set_output( uint8_t ch ){

    uint8_t temp = i2c_u8_read_reg8( PCA9536_I2C_ADDR, PCA9536_REG_CONFIG );
    temp &= ~( 1 << ch );
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_CONFIG, temp );
}

void pca9536_v_set_input( uint8_t ch ){

    uint8_t temp = i2c_u8_read_reg8( PCA9536_I2C_ADDR, PCA9536_REG_CONFIG );
    temp |= ( 1 << ch );
    i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_CONFIG, temp );
}

void pca9536_v_gpio_write( uint8_t ch, uint8_t state ){

    uint8_t temp = i2c_u8_read_reg8( PCA9536_I2C_ADDR, PCA9536_REG_OUTPUT );

    if( state ){

        temp |= ( 1 << ch );
    }
    else{

        temp &= ~( 1 << ch ); 
    }

	i2c_v_write_reg8( PCA9536_I2C_ADDR, PCA9536_REG_OUTPUT, temp );
}

bool pca9536_b_gpio_read( uint8_t ch ){

    uint8_t data = i2c_u8_read_reg8( PCA9536_I2C_ADDR, PCA9536_REG_INPUT );

    return ( ( data >> ch ) & 1 ) != 0;
}
