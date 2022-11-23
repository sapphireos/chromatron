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


#include "system.h"
#include "threading.h"
#include "timers.h"
#include "config.h"
#include "io.h"
#include "flash_fs.h"
#include "adc.h"
#include "wifi.h"

#include "status_led.h"


#include "stm32h7xx.h"

#define LED_RED_PORT    LED0_GPIO_Port
#define LED_RED_PIN     LED0_Pin
#define LED_GREEN_PORT  LED2_GPIO_Port
#define LED_GREEN_PIN   LED2_Pin
#define LED_BLUE_PORT   LED1_GPIO_Port
#define LED_BLUE_PIN    LED1_Pin


void reset_all( void ){

    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_SET);

    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin         = LED_RED_PIN;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(LED_RED_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = LED_GREEN_PIN;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(LED_GREEN_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin         = LED_BLUE_PIN;
    GPIO_InitStruct.Mode        = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    HAL_GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);
}

void hal_status_led_v_init( void ){

    reset_all();
}


void status_led_v_set( uint8_t state, uint8_t led ){

    reset_all();

    if( state == 0 ){

        return;
    }

    switch( led ){
        case STATUS_LED_BLUE:
            HAL_GPIO_WritePin(LED_BLUE_PORT, 	LED_BLUE_PIN, GPIO_PIN_RESET);
            break;

        case STATUS_LED_GREEN:
            HAL_GPIO_WritePin(LED_GREEN_PORT, 	LED_GREEN_PIN, GPIO_PIN_RESET);
            break;

        case STATUS_LED_RED:
            HAL_GPIO_WritePin(LED_RED_PORT, 	LED_RED_PIN, GPIO_PIN_RESET);
            break;

        case STATUS_LED_YELLOW:
            HAL_GPIO_WritePin(LED_GREEN_PORT, 	LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RED_PORT, 	LED_RED_PIN, GPIO_PIN_RESET);
            break;

        case STATUS_LED_PURPLE:
            HAL_GPIO_WritePin(LED_BLUE_PORT, 	LED_BLUE_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RED_PORT, 	LED_RED_PIN, GPIO_PIN_RESET);
            break;

        case STATUS_LED_TEAL:
            HAL_GPIO_WritePin(LED_GREEN_PORT, 	LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_BLUE_PORT, 	LED_BLUE_PIN, GPIO_PIN_RESET);
            break;

        case STATUS_LED_WHITE:
            HAL_GPIO_WritePin(LED_GREEN_PORT, 	LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RED_PORT, 	LED_RED_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_BLUE_PORT, 	LED_BLUE_PIN, GPIO_PIN_RESET);
            break;

        default:
            break;
    }
}
