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

// MCP23008 I2C port expander


#include "sapphire.h"
#include "mcp23008.h"

#ifdef ESP32

static uint8_t gpio_output_state;


void mcp23008_v_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    for( uint8_t i = 0; i < MCP23008_N_PINS; i++ ){
        
        mcp23008_v_set_mode( i, IO_MODE_INPUT );
        mcp23008_v_digital_write( i, 0 );
    }
}

void mcp23008_v_set_mode( uint8_t pin, io_mode_t8 mode ){

    if( pin >= MCP23008_N_PINS ){

        return;
    }

    // read io direction register
    uint8_t io_dir = mcp23008_u8_reg_read( MCP23008_REG_IODIR );

    if( mode >= IO_MODE_OUTPUT ){

        // set 0 for output
        io_dir &= ~( 1 << pin );
    }
    else{

        // set 1 for input
        io_dir |= ( 1 << pin );
    }

    mcp23008_v_reg_write( MCP23008_REG_IODIR, io_dir );

    // configure pullups
    uint8_t gppu = mcp23008_u8_reg_read( MCP23008_REG_GPPU );

    if( mode == IO_MODE_INPUT_PULLUP ){

        gppu |= ( 1 << pin );
    }
    else{

        gppu &= ~( 1 << pin );
    }

    mcp23008_v_reg_write( MCP23008_REG_GPPU, gppu );
}

void mcp23008_v_set_mask( uint8_t mask ){

    gpio_output_state |= mask;

    mcp23008_v_reg_write( MCP23008_REG_GPIO, gpio_output_state );
}

void mcp23008_v_clear_mask( uint8_t mask ){

    gpio_output_state &= ~mask;

    mcp23008_v_reg_write( MCP23008_REG_GPIO, gpio_output_state );
}

void mcp23008_v_write_mask( uint8_t mask, uint8_t state ){

    gpio_output_state &= ~mask;
    gpio_output_state |= state;

    mcp23008_v_reg_write( MCP23008_REG_GPIO, gpio_output_state );
}

void mcp23008_v_digital_write( uint8_t pin, bool state ){

    if( pin >= MCP23008_N_PINS ){

        return;
    }

    if( state ){

        gpio_output_state |= ( 1 << pin );
    }
    else{

        gpio_output_state &= ~( 1 << pin );
    }

    mcp23008_v_reg_write( MCP23008_REG_GPIO, gpio_output_state );
}

bool mcp23008_b_digital_read( uint8_t pin ){

    if( pin >= MCP23008_N_PINS ){

        return FALSE;
    }

    uint8_t port = mcp23008_u8_reg_read( MCP23008_REG_GPIO );

    return ( port & ( 1 << pin ) ) != 0;
}

void mcp23008_v_reg_write( uint8_t addr, uint8_t data ){

    i2c_v_write_reg8( MCP23008_I2C_ADDR, addr, data );
}

uint8_t mcp23008_u8_reg_read( uint8_t addr ){

    return i2c_u8_read_reg8( MCP23008_I2C_ADDR, addr );
}



#endif