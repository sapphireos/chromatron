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

typedef struct{
    GPIO_TypeDef *port;
    uint32_t pin;
} hal_io_ch_t;

static const hal_io_ch_t board_io[IO_PIN_COUNT] = {
#ifdef BOARD_CHROMATRONX
    { I2C1_SDA_GPIO_Port, I2C1_SDA_Pin }, // IO_PIN_GPIOSDA
    { I2C1_SCL_GPIO_Port, I2C1_SCL_Pin }, // IO_PIN_GPIOSCL

    { SPI4_SCK_GPIO_Port, SPI4_SCK_Pin }, // IO_PIN_GPIOSCK
    { SPI4_MOSI_GPIO_Port, SPI4_MOSI_Pin }, // IO_PIN_GPIOMOSI
    { SPI4_MISO_GPIO_Port, SPI4_MISO_Pin }, // IO_PIN_GPIOMISO
#else
    { GPIOC, GPIO_PIN_5 }, // IO_PIN_GPIOA0
    { GPIOC, GPIO_PIN_4 }, // IO_PIN_GPIOA1
    { GPIOA, GPIO_PIN_6 }, // IO_PIN_GPIOA2
    { GPIOA, GPIO_PIN_5 }, // IO_PIN_GPIOA3
    { GPIOA, GPIO_PIN_4 }, // IO_PIN_GPIOA4
    { GPIOA, GPIO_PIN_7 }, // IO_PIN_GPIOA5
    { GPIOE, GPIO_PIN_2 }, // IO_PIN_GPIOSCK
    { GPIOE, GPIO_PIN_6 }, // IO_PIN_GPIOMOSI
    { GPIOE, GPIO_PIN_5 }, // IO_PIN_GPIOMISO
    { GPIOB, GPIO_PIN_5 }, // IO_PIN_GPIORX
    { GPIOB, GPIO_PIN_6 }, // IO_PIN_GPIOTX
    { GPIOB, GPIO_PIN_13 }, // IO_PIN_PIX_CLK
    { GPIOB, GPIO_PIN_14 }, // IO_PIN_PIX_DAT
    { GPIOB, GPIO_PIN_9 }, // IO_PIN_GPIOSDA
    { GPIOB, GPIO_PIN_8 }, // IO_PIN_GPIOSCL
    { GPIOE, GPIO_PIN_7 }, // IO_PIN_GPIO5
    { GPIOE, GPIO_PIN_8 }, // IO_PIN_GPIO6
    { GPIOE, GPIO_PIN_9 }, // IO_PIN_GPIO9
    { GPIOE, GPIO_PIN_10 }, // IO_PIN_GPIO10
    { GPIOE, GPIO_PIN_11 }, // IO_PIN_GPIO11
    { GPIOE, GPIO_PIN_12 }, // IO_PIN_GPIO12
    { GPIOE, GPIO_PIN_15 }, // IO_PIN_GPIO13
#endif
};



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

void hal_io_v_get_port( uint8_t pin, GPIO_TypeDef **port, uint32_t *pin_number ){

    ASSERT( pin < IO_PIN_COUNT );

    *port = board_io[pin].port;
    *pin_number = board_io[pin].pin;
}

void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

	GPIO_TypeDef *GPIOx;
    gpio_config[pin].Mode        = GPIO_MODE_INPUT;
    gpio_config[pin].Speed       = GPIO_SPEED_FREQ_HIGH;
    gpio_config[pin].Pull        = GPIO_NOPULL;

    hal_io_v_get_port( pin, &GPIOx, &gpio_config[pin].Pin );

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
    hal_io_v_get_port( pin, &GPIOx, &gpio_pin );

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
    hal_io_v_get_port( pin, &GPIOx, &gpio_pin );

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
