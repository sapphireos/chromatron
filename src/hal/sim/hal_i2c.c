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

#include "cpu.h"
#include "system.h"
#include "i2c.h"

// sim: no physical I2C: all transfers are no-ops

void i2c_v_init( i2c_baud_t8 baud ){

}

void i2c_v_set_pins( uint8_t clock, uint8_t data ){

}

void i2c_v_write( uint8_t dev_addr, const uint8_t *src, uint8_t len ){

}

void i2c_v_read( uint8_t dev_addr, uint8_t *dst, uint8_t len ){

    for( uint8_t i = 0; i < len; i++ ){
        dst[i] = 0;
    }
}

void i2c_v_mem_write( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, const uint8_t *src, uint8_t len, uint16_t delay_ms ){

}

void i2c_v_mem_read( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, uint8_t *dst, uint8_t len, uint16_t delay_ms ){

    for( uint8_t i = 0; i < len; i++ ){
        dst[i] = 0;
    }
}
