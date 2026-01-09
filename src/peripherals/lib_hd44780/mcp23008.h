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

#ifndef _MCP23008_H_
#define _MCP23008_H_

#define MCP23008_N_PINS         8

#define MCP23008_I2C_ADDR       0x20

// registers
#define MCP23008_REG_IODIR      0x00
#define MCP23008_REG_IPOL       0x01
#define MCP23008_REG_GPINTEN    0x02
#define MCP23008_REG_DEFVAL     0x03
#define MCP23008_REG_INTCON     0x04
#define MCP23008_REG_IOCON      0x05
#define MCP23008_REG_GPPU       0x06
#define MCP23008_REG_INTF       0x07
#define MCP23008_REG_INTCAP     0x08
#define MCP23008_REG_GPIO       0x09
#define MCP23008_REG_OLAT       0x0A


void mcp23008_v_init( void );
void mcp23008_v_set_mode( uint8_t pin, io_mode_t8 mode );
void mcp23008_v_set_mask( uint8_t mask );
void mcp23008_v_clear_mask( uint8_t mask );
void mcp23008_v_digital_write( uint8_t pin, bool state );
bool mcp23008_b_digital_read( uint8_t pin );
void mcp23008_v_reg_write( uint8_t addr, uint8_t data );
uint8_t mcp23008_u8_reg_read( uint8_t addr );

#endif

