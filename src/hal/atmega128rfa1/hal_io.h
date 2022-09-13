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

#ifndef _HAL_IO_H
#define _HAL_IO_H

#include "system.h"
#include "io_intf.h"
#include "cpu.h"

uint8_t io_u8_get_pin( uint8_t pin );
#define IO_PIN_HW_ID        28

#define IO_PIN_LED2         27
#define IO_PIN_LED1         26
#define IO_PIN_LED0         25

#define IO_PIN_S3           24
#define IO_PIN_S2           23
#define IO_PIN_S1           22
#define IO_PIN_S0           21

#define IO_PIN_GPIO7        20
#define IO_PIN_GPIO6        19
#define IO_PIN_GPIO5        18
#define IO_PIN_GPIO4        17
#define IO_PIN_GPIO3        16
#define IO_PIN_GPIO2        15
#define IO_PIN_GPIO1        14
#define IO_PIN_GPIO0        13

#define IO_PIN_ADC7         12
#define IO_PIN_ADC6         11
#define IO_PIN_ADC5         10
#define IO_PIN_ADC4         9
#define IO_PIN_ADC3         8
#define IO_PIN_ADC2         7
#define IO_PIN_ADC1         6
#define IO_PIN_ADC0         5

#define IO_PIN_PWM2         4
#define IO_PIN_PWM1         3
#define IO_PIN_PWM0         2

#define IO_PIN_TX           1
#define IO_PIN_RX           0

// LED color definitions
#define IO_PIN_LED_GREEN    IO_PIN_LED0
#define IO_PIN_LED_YELLOW   IO_PIN_LED1
#define IO_PIN_LED_RED      IO_PIN_LED2

#define IO_PIN_COUNT        29


// #define IO_PIN0_PORT        PORTA
// #define IO_PIN0_PINCTRL     PIN4CTRL
// #define IO_PIN0_PIN         4
// #define IO_PIN1_PORT        PORTC
// #define IO_PIN1_PINCTRL     PIN5CTRL
// #define IO_PIN1_PIN         5
// #define IO_PIN2_PORT        PORTC
// #define IO_PIN2_PINCTRL     PIN7CTRL
// #define IO_PIN2_PIN         7
// #define IO_PIN3_PORT        PORTC
// #define IO_PIN3_PINCTRL     PIN6CTRL
// #define IO_PIN3_PIN         6
// #define IO_PIN4_PORT        PORTA
// #define IO_PIN4_PINCTRL     PIN1CTRL
// #define IO_PIN4_PIN         1
// #define IO_PIN5_PORT        PORTA
// #define IO_PIN5_PINCTRL     PIN0CTRL
// #define IO_PIN5_PIN         0
// #define IO_PIN6_PORT        PORTB
// #define IO_PIN6_PINCTRL     PIN2CTRL
// #define IO_PIN6_PIN         2
// #define IO_PIN7_PORT        PORTB
// #define IO_PIN7_PINCTRL     PIN3CTRL
// #define IO_PIN7_PIN         3
// #define IO_PWM0_PORT        PORTC
// #define IO_PWM0_PINCTRL     PIN0CTRL
// #define IO_PWM0_PIN         0
// #define IO_PWM1_PORT        PORTC
// #define IO_PWM1_PINCTRL     PIN1CTRL
// #define IO_PWM1_PIN         1
// #define IO_PWM2_PORT        PORTC
// #define IO_PWM2_PINCTRL     PIN2CTRL
// #define IO_PWM2_PIN         2
// #define IO_PWM3_PORT        PORTC
// #define IO_PWM3_PINCTRL     PIN3CTRL
// #define IO_PWM3_PIN         3
// #define IO_LED_RED_PORT     PORTC
// #define IO_LED_RED_PINCTRL  PIN4CTRL
// #define IO_LED_RED_PIN      4
// #define IO_LED_GREEN_PORT    PORTD
// #define IO_LED_GREEN_PINCTRL PIN5CTRL
// #define IO_LED_GREEN_PIN     5
// #define IO_LED_BLUE_PORT    PORTD
// #define IO_LED_BLUE_PINCTRL PIN4CTRL
// #define IO_LED_BLUE_PIN     4



// #define HW_REV0_PORT        PORTB
// #define HW_REV0_PINCTRL     PIN0CTRL
// #define HW_REV0_PIN         0
// #define HW_REV1_PORT        PORTB
// #define HW_REV1_PINCTRL     PIN1CTRL
// #define HW_REV1_PIN         1


#endif
