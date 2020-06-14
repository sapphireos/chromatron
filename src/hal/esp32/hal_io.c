/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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
#include "target.h"

#include "hal_io.h"

static io_mode_t8 io_modes[IO_PIN_COUNT];

static const gpio_num_t gpios[IO_PIN_COUNT] = {
    GPIO_NUM_13, // IO_PIN_13_A12 
    GPIO_NUM_12, // IO_PIN_12_A11 
    GPIO_NUM_27, // IO_PIN_27_A10 
    GPIO_NUM_33, // IO_PIN_33_A9  
    GPIO_NUM_15, // IO_PIN_15_A8  
    GPIO_NUM_32, // IO_PIN_32_A7  
    GPIO_NUM_14, // IO_PIN_14_A6  
    GPIO_NUM_22, // IO_PIN_22_SCL 
    GPIO_NUM_23, // IO_PIN_23_SDA 
    GPIO_NUM_21, // IO_PIN_21     
    GPIO_NUM_17, // IO_PIN_17_TX  
    GPIO_NUM_16, // IO_PIN_16_RX  
    GPIO_NUM_19, // IO_PIN_19_MISO
    GPIO_NUM_18, // IO_PIN_18_MOSI
    GPIO_NUM_5,  // IO_PIN_5_SCK  
    GPIO_NUM_4,  // IO_PIN_4_A5   
    GPIO_NUM_36, // IO_PIN_36_A4  
    GPIO_NUM_36, // IO_PIN_39_A3  
    GPIO_NUM_36, // IO_PIN_34_A2  
    GPIO_NUM_25, // IO_PIN_25_A1  
    GPIO_NUM_26, // IO_PIN_26_A0  
};

void io_v_init( void ){


}

uint8_t io_u8_get_board_rev( void ){

    return 0;
}


void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

    ASSERT( pin < IO_PIN_COUNT );

    io_modes[pin] = mode;

    gpio_num_t gpio = gpios[pin];
	
    if( mode == IO_MODE_INPUT ){

        gpio_set_direction( gpio, GPIO_MODE_INPUT );
        gpio_set_pull_mode( gpio, GPIO_FLOATING );
    }
    else if( mode == IO_MODE_INPUT_PULLUP ){

        gpio_set_direction( gpio, GPIO_MODE_INPUT );
        gpio_set_pull_mode( gpio, GPIO_PULLUP_ONLY );
    }
    else if( mode == IO_MODE_INPUT_PULLDOWN ){

        gpio_set_direction( gpio, GPIO_MODE_INPUT );
        gpio_set_pull_mode( gpio, GPIO_PULLDOWN_ONLY );
    }
    else if( mode == IO_MODE_OUTPUT ){

        gpio_set_direction( gpio, GPIO_MODE_OUTPUT );   
    }
    else if( mode == IO_MODE_OUTPUT_OPEN_DRAIN ){

        gpio_set_direction( gpio, GPIO_MODE_OUTPUT_OD );   
    }
    else{

        ASSERT( FALSE );
    }
}


io_mode_t8 io_u8_get_mode( uint8_t pin ){
	
    ASSERT( pin < IO_PIN_COUNT );
    
    return io_modes[pin];	
}

void io_v_digital_write( uint8_t pin, bool state ){

    ASSERT( pin < IO_PIN_COUNT );

    gpio_num_t gpio = gpios[pin];
    
    gpio_set_level( gpio, state );	
}

bool io_b_digital_read( uint8_t pin ){
    
    ASSERT( pin < IO_PIN_COUNT );

    gpio_num_t gpio = gpios[pin];
    
    return gpio_get_level( gpio );
}

bool io_b_button_down( void ){

    return FALSE;
}

void io_v_disable_jtag( void ){

}

void io_v_enable_interrupt(
    uint8_t int_number,
    io_int_handler_t handler,
    io_int_mode_t8 mode )
{

}

void io_v_disable_interrupt( uint8_t int_number )
{

}

void hal_io_v_set_esp_led( bool state ){

    io_v_digital_write( IO_PIN_LED, state );
}
