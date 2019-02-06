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


#include "cpu.h"

#include "system.h"
#include "target.h"

#include "hal_io.h"


static GPIO_InitTypeDef gpio_config[IO_PIN_COUNT];


void io_v_init( void ){

    // set all other pins to inputs pulled down
    for( uint8_t i = 0; i < IO_PIN_COUNT; i++ ){

        io_v_set_mode( i, IO_MODE_INPUT_PULLDOWN );
    }
}

uint8_t io_u8_get_board_rev( void ){

    return 0;
}

void get_port( uint8_t pin, GPIO_TypeDef **port, uint32_t *pin_number ){

    switch( pin ){
        case IO_PIN_0:
            *port = IO_PIN0_PORT;
            *pin_number = IO_PIN0_PIN;
            break;

        case IO_PIN_1:
            *port = IO_PIN1_PORT;
            *pin_number = IO_PIN1_PIN;
            break;

        case IO_PIN_2:
            *port = IO_PIN2_PORT;
            *pin_number = IO_PIN2_PIN;
            break;

        case IO_PIN_3:
            *port = IO_PIN3_PORT;
            *pin_number = IO_PIN3_PIN;
            break;

        case IO_PIN_4:
            *port = IO_PIN4_PORT;
            *pin_number = IO_PIN4_PIN;
            break;

        case IO_PIN_5:
            *port = IO_PIN5_PORT;
            *pin_number = IO_PIN5_PIN;
            break;

        case IO_PIN_6:
            *port = IO_PIN6_PORT;
            *pin_number = IO_PIN6_PIN;
            break;

        case IO_PIN_7:
            *port = IO_PIN7_PORT;
            *pin_number = IO_PIN7_PIN;
            break;

        case IO_PIN_8:
            *port = IO_PIN8_PORT;
            *pin_number = IO_PIN8_PIN;
            break;

        case IO_PIN_9:
            *port = IO_PIN9_PORT;
            *pin_number = IO_PIN9_PIN;
            break;

        case IO_PIN_10:
            *port = IO_PIN10_PORT;
            *pin_number = IO_PIN10_PIN;
            break;

        case IO_PIN_CS:
            *port = IO_PINCS_PORT;
            *pin_number = IO_PINCS_PIN;
            break;

        case IO_PIN_T0:
            *port = IO_PINT0_PORT;
            *pin_number = IO_PINT0_PIN;
            break;

        case IO_PIN_T1:
            *port = IO_PINT1_PORT;
            *pin_number = IO_PINT1_PIN;
            break;

        default:
            ASSERT( FALSE );
            return;
            break;
    }
}

void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

	GPIO_TypeDef *GPIOx;
    gpio_config[pin].Mode        = GPIO_MODE_INPUT;
    gpio_config[pin].Speed       = GPIO_SPEED_FREQ_HIGH;
    gpio_config[pin].Pull        = GPIO_NOPULL;

    get_port( pin, &GPIOx, &gpio_config[pin].Pin );

    if( mode >= IO_MODE_OUTPUT ){

        if( mode == IO_MODE_OUTPUT_OPEN_DRAIN ){

            gpio_config[pin].Mode = GPIO_MODE_OUTPUT_OD;
        }
        else{

        	gpio_config[pin].Mode = GPIO_MODE_OUTPUT_PP;
        }
    }
    else{

    	gpio_config[pin].Mode = GPIO_MODE_INPUT;

        if( mode == IO_MODE_INPUT_PULLUP ){

        	gpio_config[pin].Pull = GPIO_PULLUP;
        }
        else if( mode == IO_MODE_INPUT_PULLDOWN ){

        	gpio_config[pin].Pull = GPIO_PULLDOWN;
        }
    }

    HAL_GPIO_Init( GPIOx, &gpio_config[pin] );
}


io_mode_t8 io_u8_get_mode( uint8_t pin ){

    if( gpio_config[pin].Mode == GPIO_MODE_OUTPUT_PP ){

        return IO_MODE_OUTPUT;
    }
    else if( gpio_config[pin].Mode == GPIO_MODE_OUTPUT_OD ){

        return IO_MODE_OUTPUT_OPEN_DRAIN;
    }
    else{   

        if( gpio_config[pin].Pull == GPIO_PULLUP ){

    		return IO_MODE_INPUT_PULLUP;
    	}
        else if( gpio_config[pin].Pull == GPIO_PULLDOWN ){

    		return IO_MODE_INPUT_PULLDOWN;
    	}
    	else{

    		return IO_MODE_INPUT;	
    	}
    }

    return IO_MODE_INPUT;
}

void io_v_digital_write( uint8_t pin, bool state ){

	GPIO_TypeDef *GPIOx;
	uint32_t gpio_pin;
    get_port( pin, &GPIOx, &gpio_pin );

    if( state ){

    	HAL_GPIO_WritePin( GPIOx, gpio_pin, GPIO_PIN_SET );
    }
    else{

    	HAL_GPIO_WritePin( GPIOx, gpio_pin, GPIO_PIN_RESET );
    }
}

bool io_b_digital_read( uint8_t pin ){

	GPIO_TypeDef *GPIOx;
	uint32_t gpio_pin;
    get_port( pin, &GPIOx, &gpio_pin );

    return HAL_GPIO_ReadPin( GPIOx, gpio_pin ) == GPIO_PIN_SET;
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
