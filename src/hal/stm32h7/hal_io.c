/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

    GPIO_InitTypeDef GPIO_InitStruct;

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOE, WIFI_BOOT_Pin|WIFI_RST_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOF, WIFI_PD_Pin|WIFI_SS_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, LED1_Pin|LED0_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : WIFI_BOOT_Pin WIFI_RST_Pin */
    GPIO_InitStruct.Pin = WIFI_BOOT_Pin|WIFI_RST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /*Configure GPIO pin : WIFI_RX_Ready_Pin */
    GPIO_InitStruct.Pin = WIFI_RX_Ready_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(WIFI_RX_Ready_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : USER_Btn_Pin */
    GPIO_InitStruct.Pin = USER_Btn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : WIFI_PD_Pin WIFI_SS_Pin */
    GPIO_InitStruct.Pin = WIFI_PD_Pin|WIFI_SS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /*Configure GPIO pins : LED1_Pin LED0_Pin */
    GPIO_InitStruct.Pin = LED1_Pin|LED0_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : LD3_Pin LD2_Pin */
    GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
    GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : USB_OverCurrent_Pin */
    GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : LED2_Pin */
    GPIO_InitStruct.Pin = LED2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED2_GPIO_Port, &GPIO_InitStruct);



    // set all other pins to inputs pulled down
    for( uint8_t i = 0; i < IO_PIN_COUNT; i++ ){

        io_v_set_mode( i, IO_MODE_INPUT_PULLDOWN );
    }
}

uint8_t io_u8_get_board_rev( void ){

    return 0;
}


void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

	GPIO_TypeDef *GPIOx;
    gpio_config[pin].Mode        = GPIO_MODE_INPUT;
    gpio_config[pin].Speed       = GPIO_SPEED_FREQ_LOW;
    gpio_config[pin].Pull        = GPIO_NOPULL;

	switch( pin ){
        case IO_PIN_0:
        	GPIOx = IO_PIN0_PORT;
        	gpio_config[pin].Pin = IO_PIN0_PIN;
            break;

        case IO_PIN_1:
        	GPIOx = IO_PIN1_PORT;
        	gpio_config[pin].Pin = IO_PIN1_PIN;
            break;

        case IO_PIN_2:
        	GPIOx = IO_PIN2_PORT;
        	gpio_config[pin].Pin = IO_PIN2_PIN;
            break;

        case IO_PIN_3:
        	GPIOx = IO_PIN3_PORT;
        	gpio_config[pin].Pin = IO_PIN3_PIN;
            break;

        case IO_PIN_4:
        	GPIOx = IO_PIN4_PORT;
        	gpio_config[pin].Pin = IO_PIN4_PIN;
            break;

        case IO_PIN_5:
        	GPIOx = IO_PIN5_PORT;
        	gpio_config[pin].Pin = IO_PIN5_PIN;
            break;

        default:
            return;
            break;
    }


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

	GPIO_TypeDef *GPIOx;
	uint32_t gpio_pin;

	switch( pin ){
        case IO_PIN_0:
        	GPIOx = IO_PIN0_PORT;
        	gpio_pin = IO_PIN0_PIN;
            break;

        case IO_PIN_1:
        	GPIOx = IO_PIN1_PORT;
        	gpio_pin = IO_PIN1_PIN;
            break;

        case IO_PIN_2:
        	GPIOx = IO_PIN2_PORT;
        	gpio_pin = IO_PIN2_PIN;
            break;

        case IO_PIN_3:
        	GPIOx = IO_PIN3_PORT;
        	gpio_pin = IO_PIN3_PIN;
            break;

        case IO_PIN_4:
        	GPIOx = IO_PIN4_PORT;
        	gpio_pin = IO_PIN4_PIN;
            break;

        case IO_PIN_5:
        	GPIOx = IO_PIN5_PORT;
        	gpio_pin = IO_PIN5_PIN;
            break;

        default:
            return IO_MODE_INPUT;
            break;
    }


    if( gpio_config[gpio_pin].Mode == GPIO_MODE_OUTPUT_PP ){

        return IO_MODE_OUTPUT;
    }
    else if( gpio_config[gpio_pin].Mode == GPIO_MODE_OUTPUT_OD ){

        return IO_MODE_OUTPUT_OPEN_DRAIN;
    }
    else{   

        if( gpio_config[gpio_pin].Pull == GPIO_PULLUP ){

    		return IO_MODE_INPUT_PULLUP;
    	}
        else if( gpio_config[gpio_pin].Pull == GPIO_PULLDOWN ){

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

	switch( pin ){
        case IO_PIN_0:
        	GPIOx = IO_PIN0_PORT;
        	gpio_pin = IO_PIN0_PIN;
            break;

        case IO_PIN_1:
        	GPIOx = IO_PIN1_PORT;
        	gpio_pin = IO_PIN1_PIN;
            break;

        case IO_PIN_2:
        	GPIOx = IO_PIN2_PORT;
        	gpio_pin = IO_PIN2_PIN;
            break;

        case IO_PIN_3:
        	GPIOx = IO_PIN3_PORT;
        	gpio_pin = IO_PIN3_PIN;
            break;

        case IO_PIN_4:
        	GPIOx = IO_PIN4_PORT;
        	gpio_pin = IO_PIN4_PIN;
            break;

        case IO_PIN_5:
        	GPIOx = IO_PIN5_PORT;
        	gpio_pin = IO_PIN5_PIN;
            break;

        default:
            return;
            break;
    }

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

	switch( pin ){
        case IO_PIN_0:
        	GPIOx = IO_PIN0_PORT;
        	gpio_pin = IO_PIN0_PIN;
            break;

        case IO_PIN_1:
        	GPIOx = IO_PIN1_PORT;
        	gpio_pin = IO_PIN1_PIN;
            break;

        case IO_PIN_2:
        	GPIOx = IO_PIN2_PORT;
        	gpio_pin = IO_PIN2_PIN;
            break;

        case IO_PIN_3:
        	GPIOx = IO_PIN3_PORT;
        	gpio_pin = IO_PIN3_PIN;
            break;

        case IO_PIN_4:
        	GPIOx = IO_PIN4_PORT;
        	gpio_pin = IO_PIN4_PIN;
            break;

        case IO_PIN_5:
        	GPIOx = IO_PIN5_PORT;
        	gpio_pin = IO_PIN5_PIN;
            break;

        default:
            return FALSE;
            break;
    }

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
