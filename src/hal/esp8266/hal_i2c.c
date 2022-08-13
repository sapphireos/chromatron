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

#include "hal_io.h"

#include "hal_i2c.h"

#include "i2c.h"


#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

#include "logging.h"


void i2c_v_init( i2c_baud_t8 baud ){

	#ifdef ENABLE_COPROCESSOR
    
    coproc_i32_call1( OPCODE_IO_I2C_INIT, baud );
    
    #else

    #endif
}

void i2c_v_set_pins( uint8_t clock, uint8_t data ){
	
	#ifdef ENABLE_COPROCESSOR
    
    coproc_i32_call2( OPCODE_IO_I2C_SET_PINS, clock, data );
    
    #else

    #endif
}

void i2c_v_write( uint8_t dev_addr, const uint8_t *src, uint8_t len ){

	#ifdef ENABLE_COPROCESSOR

	i2c_setup_t setup = {
		.dev_addr = dev_addr,
		.len = len,
	};
    
    coproc_i32_callv( OPCODE_IO_I2C_SETUP, (const uint8_t *)&setup, sizeof(setup) );
    coproc_i32_callv( OPCODE_IO_I2C_WRITE, src, len );
    
    #else

    #endif	
}

void i2c_v_read( uint8_t dev_addr, uint8_t *dst, uint8_t len ){

	#ifdef ENABLE_COPROCESSOR
    
    i2c_setup_t setup = {
		.dev_addr 	= dev_addr,
		.len 		= len,
	};
    
    coproc_i32_callv( OPCODE_IO_I2C_SETUP, (const uint8_t *)&setup, sizeof(setup) );
    coproc_i32_callp( OPCODE_IO_I2C_READ, dst, len );
    
    #else

    #endif
}

void i2c_v_mem_write( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, const uint8_t *src, uint8_t len, uint16_t delay_ms ){

    #ifdef ENABLE_COPROCESSOR
    
    i2c_setup_t setup = {
		.dev_addr 	= dev_addr,
		.len 		= len,
		.mem_addr 	= mem_addr,
		.addr_size  = addr_size,
		.delay_ms   = delay_ms,
	};
    
    coproc_i32_callv( OPCODE_IO_I2C_SETUP, (const uint8_t *)&setup, sizeof(setup) );
    coproc_i32_callv( OPCODE_IO_I2C_MEM_WRITE, src, len );
    
    #else

    #endif
}

void i2c_v_mem_read( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, uint8_t *dst, uint8_t len, uint16_t delay_ms ){

    #ifdef ENABLE_COPROCESSOR
    
    i2c_setup_t setup = {
		.dev_addr 	= dev_addr,
		.len 		= len,
		.mem_addr 	= mem_addr,
		.addr_size  = addr_size,
		.delay_ms   = delay_ms,
	};
    
    coproc_i32_callv( OPCODE_IO_I2C_SETUP, (const uint8_t *)&setup, sizeof(setup) );
    coproc_i32_callp( OPCODE_IO_I2C_MEM_READ, dst, len );
    
    #else

    #endif
}
