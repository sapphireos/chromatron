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

#include "hal_i2c.h"


void i2c_v_write_reg8( uint8_t address, uint8_t reg_addr, uint8_t data ){
    
    uint8_t cmd[2];
    cmd[0] = reg_addr;
    cmd[1] = data;

    i2c_v_write( address, cmd, sizeof(cmd) );
}

uint8_t i2c_u8_read_reg8( uint8_t address, uint8_t reg_addr ){
    
    uint8_t data = 0;

    i2c_v_write( address, &reg_addr, sizeof(reg_addr) );
    i2c_v_read( address, &data, sizeof(data) );

    return data;
}
