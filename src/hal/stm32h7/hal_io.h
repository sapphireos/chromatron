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

#ifndef _HAL_IO_H
#define _HAL_IO_H

#include "system.h"
#include "io_intf.h"
#include "cpu.h"

#include "stm32h7xx_hal.h"

#define IO_PIN_0       		0 // PA4
#define IO_PIN_1      		1 // PA5
#define IO_PIN_2       		2 // PA6
#define IO_PIN_3       		3 // PA7
#define IO_PIN_4       		4 // PB10
#define IO_PIN_5       		5 // PB11
#define IO_PIN_COUNT        6

#define IO_PIN0_PORT        GPIOA
#define IO_PIN0_PIN         GPIO_PIN_4
#define IO_PIN1_PORT        GPIOA
#define IO_PIN1_PIN         GPIO_PIN_5
#define IO_PIN2_PORT        GPIOA
#define IO_PIN2_PIN         GPIO_PIN_6
#define IO_PIN3_PORT        GPIOA
#define IO_PIN3_PIN         GPIO_PIN_7
#define IO_PIN4_PORT        GPIOB
#define IO_PIN4_PIN         GPIO_PIN_10
#define IO_PIN5_PORT        GPIOB
#define IO_PIN5_PIN         GPIO_PIN_11



#define VMON_Pin GPIO_PIN_0
#define VMON_GPIO_Port GPIOC
#define LED0_Pin GPIO_PIN_1
#define LED0_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_2
#define LED1_GPIO_Port GPIOA
#define LED2_Pin GPIO_PIN_3
#define LED2_GPIO_Port GPIOA
#define FSPI_CLK_Pin GPIO_PIN_2
#define FSPI_CLK_GPIO_Port GPIOB
#define GPIO4_Pin GPIO_PIN_10
#define GPIO4_GPIO_Port GPIOB
#define GPIO5_Pin GPIO_PIN_11
#define GPIO5_GPIO_Port GPIOB
#define PIX_CLK2_Pin GPIO_PIN_13
#define PIX_CLK2_GPIO_Port GPIOB
#define PIX_DAT2_Pin GPIO_PIN_15
#define PIX_DAT2_GPIO_Port GPIOB
#define WIFI_TXD_Pin GPIO_PIN_6
#define WIFI_TXD_GPIO_Port GPIOC
#define WIFI_RXD_Pin GPIO_PIN_7
#define WIFI_RXD_GPIO_Port GPIOC
#define FSPI_MISO_Pin GPIO_PIN_9
#define FSPI_MISO_GPIO_Port GPIOC
#define COMM_TX_Pin GPIO_PIN_9
#define COMM_TX_GPIO_Port GPIOA
#define COMM_RX_Pin GPIO_PIN_10
#define COMM_RX_GPIO_Port GPIOA
#define FSPI_MOSI_Pin GPIO_PIN_10
#define FSPI_MOSI_GPIO_Port GPIOC
#define WIFI_PD_Pin GPIO_PIN_12
#define WIFI_PD_GPIO_Port GPIOC
#define PIX_CLK_Pin GPIO_PIN_3
#define PIX_CLK_GPIO_Port GPIOB
#define WIFI_RST_Pin GPIO_PIN_4
#define WIFI_RST_GPIO_Port GPIOB
#define PIX_DAT_Pin GPIO_PIN_5
#define PIX_DAT_GPIO_Port GPIOB
#define FLASH_CS_Pin GPIO_PIN_6
#define FLASH_CS_GPIO_Port GPIOB
#define WIFI_BOOT_Pin GPIO_PIN_8
#define WIFI_BOOT_GPIO_Port GPIOB
#define WIFI_SS_Pin GPIO_PIN_9
#define WIFI_SS_GPIO_Port GPIOB
// #define WIFI_RX_Ready_Pin GPIO_PIN_4
// #define WIFI_RX_Ready_Port GPIOC
#define WIFI_RX_Ready_Pin IO_PIN5_PIN
#define WIFI_RX_Ready_Port IO_PIN5_PORT

#endif
