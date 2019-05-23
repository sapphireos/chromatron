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

#ifndef _HAL_IO_H
#define _HAL_IO_H

#include "system.h"
#include "io_intf.h"
#include "cpu.h"

#include "stm32h7xx_hal.h"

void hal_io_v_get_port( uint8_t pin, GPIO_TypeDef **port, uint32_t *pin_number );

#ifdef BOARD_CHROMATRONX

#define IO_PIN_GPIOSDA      0  
#define IO_PIN_GPIOSCL      1  
#define IO_PIN_GPIOSCK      2   
#define IO_PIN_GPIOMOSI     3   
#define IO_PIN_GPIOMISO     4   

#define IO_PIN_COUNT		5

#define IO_PIN_ANALOG_COUNT 0

// vmon is probably wrong
#define VMON_Pin 				GPIO_PIN_5
#define VMON_GPIO_Port 			GPIOC



// Chromatron X Analog
// ADC12_INP8
#define MEAS1_Pin 				PIO_PIN_5
#define MEAS1_GPIO_Port 		GPIOC

#define MEAS2_Pin 				GPIO_PIN_1
#define MEAS2_GPIO_Port 		GPIOC

#define MEAS3_Pin 				GPIO_PIN_0
#define MEAS3_GPIO_Port 		GPIOC

#define MEAS4_Pin 				GPIO_PIN_1
#define MEAS4_GPIO_Port 		GPIOA


#define PIX_CLK_3_Pin 			GPIO_PIN_2
#define PIX_CLK_3_GPIO_Port 	GPIOE
#define WIFI_BOOT_Pin 			GPIO_PIN_3
#define WIFI_BOOT_GPIO_Port 	GPIOE
#define WIFI_RST_Pin 			GPIO_PIN_4
#define WIFI_RST_GPIO_Port 		GPIOE
#define WIFI_RX_Ready_Pin 		GPIO_PIN_5
#define WIFI_RX_Ready_Port 		GPIOE
#define PIX_DAT_3_Pin 			GPIO_PIN_6
#define PIX_DAT_3_GPIO_Port 	GPIOE
#define USER_Btn_Pin 			GPIO_PIN_13
#define USER_Btn_GPIO_Port 		GPIOC
#define WIFI_PD_Pin 			GPIO_PIN_0
#define WIFI_PD_GPIO_Port 		GPIOF
#define WIFI_SS_Pin 			GPIO_PIN_1
#define WIFI_SS_GPIO_Port 		GPIOF
#define AUX_UART_RX_Pin 		GPIO_PIN_6
#define AUX_UART_RX_GPIO_Port 	GPIOF
#define PIX_CLK_4_Pin 			GPIO_PIN_7
#define PIX_CLK_4_GPIO_Port 	GPIOF
#define MCO_Pin 				GPIO_PIN_0
#define MCO_GPIO_Port 			GPIOH
#define LED1_Pin 				GPIO_PIN_2
#define LED1_GPIO_Port 			GPIOC

#define PIX_DAT_1_Pin 			GPIO_PIN_3
#define PIX_DAT_1_GPIO_Port 	GPIOC
#define PIX_CLK_0_Pin 			GPIO_PIN_5
#define PIX_CLK_0_GPIO_Port 	GPIOA
#define PIX_DAT_5_Pin 			GPIO_PIN_5
#define PIX_DAT_5_GPIO_Port 	GPIOB
#define PIX_DAT_2_Pin 			GPIO_PIN_2
#define PIX_DAT_2_GPIO_Port 	GPIOB
#define PIX_DAT_4_Pin 			GPIO_PIN_11
#define PIX_DAT_4_GPIO_Port 	GPIOF
#define LD3_Pin 				GPIO_PIN_14
#define LD3_GPIO_Port 			GPIOB
#define CMD_USART_TX_Pin 		GPIO_PIN_8
#define CMD_USART_TX_GPIO_Port 	GPIOD
#define CMD_USART_RX_Pin 		GPIO_PIN_9
#define CMD_USART_RX_GPIO_Port 	GPIOD
#define USB_PowerSwitchOn_Pin 	GPIO_PIN_6
#define USB_PowerSwitchOn_GPIO_Port GPIOG
#define USB_OverCurrent_Pin 	GPIO_PIN_7
#define USB_OverCurrent_GPIO_Port GPIOG
#define AUX_SPI_MOSI_Pin 		GPIO_PIN_6
#define AUX_SPI_MOSI_GPIO_Port 	GPIOC
#define AUX_SPI_MISO_Pin 		GPIO_PIN_7
#define AUX_SPI_MISO_GPIO_Port 	GPIOC
#define AUX_SPI_SCK_Pin 		GPIO_PIN_8
#define AUX_SPI_SCK_GPIO_Port 	GPIOC
#define PIX_CLK_6_Pin 			GPIO_PIN_8
#define PIX_CLK_6_GPIO_Port 	GPIOA
#define SWDIO_Pin 				GPIO_PIN_13
#define SWDIO_GPIO_Port 		GPIOA
#define SWCLK_Pin 				GPIO_PIN_14
#define SWCLK_GPIO_Port 		GPIOA
#define AUX_UART_TX_Pin 		GPIO_PIN_15
#define AUX_UART_TX_GPIO_Port 	GPIOA
#define PIX_CLK_2_Pin 			GPIO_PIN_10
#define PIX_CLK_2_GPIO_Port 	GPIOC
#define LED2_Pin 				GPIO_PIN_11
#define LED2_GPIO_Port 			GPIOC
#define PIX_DAT_9_Pin 			GPIO_PIN_12
#define PIX_DAT_9_GPIO_Port 	GPIOC
#define PIX_CLK_1_Pin 			GPIO_PIN_3
#define PIX_CLK_1_GPIO_Port 	GPIOD
#define LED0_Pin 				GPIO_PIN_4
#define LED0_GPIO_Port 			GPIOD
#define PIX_DAT_0_Pin 			GPIO_PIN_7
#define PIX_DAT_0_GPIO_Port 	GPIOD
#define PIX_CLK_5_Pin 			GPIO_PIN_13
#define PIX_CLK_5_GPIO_Port 	GPIOG
#define PIX_DAT_6_Pin 			GPIO_PIN_6
#define PIX_DAT_6_GPIO_Port 	GPIOB
#define LD2_Pin 				GPIO_PIN_7
#define LD2_GPIO_Port 			GPIOB
#define WIFI_RXD_Pin 			GPIO_PIN_0
#define WIFI_RXD_GPIO_Port 		GPIOE
#define WIFI_TXD_Pin 			GPIO_PIN_1
#define WIFI_TXD_GPIO_Port 		GPIOE
#define I2C1_SCL_Pin 			GPIO_PIN_8
#define I2C1_SCL_GPIO_Port 		GPIOB
#define I2C1_SDA_Pin 			GPIO_PIN_9
#define I2C1_SDA_GPIO_Port 		GPIOB
#define SPI4_MOSI_Pin 			GPIO_PIN_6
#define SPI4_MOSI_GPIO_Port 	GPIOE
#define SPI4_MISO_Pin 			GPIO_PIN_5
#define SPI4_MISO_GPIO_Port 	GPIOE
#define SPI4_SCK_Pin 			GPIO_PIN_2
#define SPI4_SCK_GPIO_Port 		GPIOE

#else

// NG

#define IO_PIN_GPIOA0       0 	// PC5 - ADC12_INP8
#define IO_PIN_GPIOA1       1	// PC4 - ADC12_INP4
#define IO_PIN_GPIOA2       2   // PA6 - I2S1_SDI - ADC12_INP3
#define IO_PIN_GPIOA3       3   // PA5 - I2S1_CK - ADC12_INP19
#define IO_PIN_GPIOA4       4   // PA4 - I2S1_WS - ADC12_INP18
#define IO_PIN_GPIOA5       5   // PA7 - ADC12_INP7
#define IO_PIN_GPIOSCK      6   // PE2
#define IO_PIN_GPIOMOSI     7   // PE6
#define IO_PIN_GPIOMISO     8   // PE5
#define IO_PIN_GPIORX       9   // PB5
#define IO_PIN_GPIOTX       10  // PB6
#define IO_PIN_PIX_CLK      11  // PB13
#define IO_PIN_PIX_DAT      12  // PB14
#define IO_PIN_GPIOSDA      13  // PB9
#define IO_PIN_GPIOSCL      14  // PB8
#define IO_PIN_GPIO5        15  // PE7
#define IO_PIN_GPIO6        16  // PE8
#define IO_PIN_GPIO9        17  // PE9 - PWM
#define IO_PIN_GPIO10       18  // PE10
#define IO_PIN_GPIO11       19  // PE11 - PWM
#define IO_PIN_GPIO12       20  // PE12
#define IO_PIN_GPIO13       21  // PE15
#define IO_PIN_COUNT		22

#define IO_PIN_ANALOG_COUNT 6


#define IO_PIN_PIX_CLK2     22  //
#define IO_PIN_PIX_DAT2     23  //

#define AUX_USART_TX_Pin 		GPIO_PIN_6
#define AUX_USART_TX_GPIO_Port 	GPIOB
#define AUX_USART_RX_Pin 		GPIO_PIN_5
#define AUX_USART_RX_GPIO_Port 	GPIOB

// WIFI_RX 		- PA0
// WIFI_TX 		- PA1
// WIFI_RST 	- PA2
// WIFI_READY 	- PA3
// WIFI_SS 		- PC1
// WIFI_BOOT 	- PC2
// WIFI_PD 		- PC3
// LED0 		- PC6
// LED2 		- PC7
// LED1 		- PC8
// USB_DM 		- PA11
// USB_DP 		- PA12
// SWDIO 		- PA13
// SWCLK 		- PA14
// VMON 		- PB1
// FSPI_SCK 	- PB2
// FSPI_CS 		- PB10
// FSPI_MOSI	- PD11
// FSPI_MISO	- PD12

#define FSPI_SCK_PIN			GPIO_PIN_2
#define FSPI_SCK_PORT			GPIOB
#define FSPI_IO0_PIN			GPIO_PIN_11
#define FSPI_IO0_PORT			GPIOD
#define FSPI_IO1_PIN			GPIO_PIN_12
#define FSPI_IO1_PORT			GPIOD
#define FSPI_CS_PIN			 	GPIO_PIN_10
#define FSPI_CS_PORT			GPIOB

#define VMON_Pin 				GPIO_PIN_1
#define VMON_GPIO_Port 			GPIOB

#define LED0_Pin 				GPIO_PIN_6
#define LED0_GPIO_Port 			GPIOC
#define LED1_Pin 				GPIO_PIN_8
#define LED1_GPIO_Port 			GPIOC
#define LED2_Pin 				GPIO_PIN_7
#define LED2_GPIO_Port 			GPIOC

#define WIFI_RXD_Pin 			GPIO_PIN_0
#define WIFI_RXD_GPIO_Port 		GPIOA
#define WIFI_TXD_Pin 			GPIO_PIN_1
#define WIFI_TXD_GPIO_Port 		GPIOA
#define WIFI_RST_Pin 			GPIO_PIN_2
#define WIFI_RST_GPIO_Port 		GPIOA
#define WIFI_PD_Pin 			GPIO_PIN_3
#define WIFI_PD_GPIO_Port 		GPIOC
#define WIFI_RX_Ready_Pin 		GPIO_PIN_3
#define WIFI_RX_Ready_Port 		GPIOA
#define WIFI_BOOT_Pin 			GPIO_PIN_2
#define WIFI_BOOT_GPIO_Port 	GPIOC
#define WIFI_SS_Pin 			GPIO_PIN_1
#define WIFI_SS_GPIO_Port 		GPIOC


// "Nuclear" board

// #define IO_PIN_0       		0 // PC0
// #define IO_PIN_1      		1 // PC3
// #define IO_PIN_2       		2 // PF12
// #define IO_PIN_3       		3 // PF13
// #define IO_PIN_4       		4 // PF14
// #define IO_PIN_5       		5 // PE9
// #define IO_PIN_6       		6 // PE13
// #define IO_PIN_7       		7 // PA3
// #define IO_PIN_8       		8 // PF5
// #define IO_PIN_9       		9 // PE3
// #define IO_PIN_10      		10 // PF7
// #define IO_PIN_CS      		11 // PE4
// #define IO_PIN_T0      		12 // PD14
// #define IO_PIN_T1      		13 // PD15
// #define IO_PIN_COUNT        14

// #define IO_PIN0_PORT        GPIOC
// #define IO_PIN0_PIN         GPIO_PIN_0
// #define IO_PIN1_PORT        GPIOC
// #define IO_PIN1_PIN         GPIO_PIN_3
// #define IO_PIN2_PORT        GPIOF
// #define IO_PIN2_PIN         GPIO_PIN_12
// #define IO_PIN3_PORT        GPIOF
// #define IO_PIN3_PIN         GPIO_PIN_13
// #define IO_PIN4_PORT        GPIOF
// #define IO_PIN4_PIN         GPIO_PIN_14
// #define IO_PIN5_PORT        GPIOE
// #define IO_PIN5_PIN         GPIO_PIN_9
// #define IO_PIN6_PORT        GPIOE
// #define IO_PIN6_PIN         GPIO_PIN_13
// #define IO_PIN7_PORT        GPIOA
// #define IO_PIN7_PIN         GPIO_PIN_3
// #define IO_PIN8_PORT        GPIOF
// #define IO_PIN8_PIN         GPIO_PIN_5
// #define IO_PIN9_PORT        GPIOE
// #define IO_PIN9_PIN         GPIO_PIN_3
// #define IO_PIN10_PORT       GPIOF
// #define IO_PIN10_PIN        GPIO_PIN_7
// #define IO_PINCS_PORT       GPIOE
// #define IO_PINCS_PIN        GPIO_PIN_4
// #define IO_PINT0_PORT       GPIOD
// #define IO_PINT0_PIN        GPIO_PIN_14
// #define IO_PINT1_PORT       GPIOD
// #define IO_PINT1_PIN        GPIO_PIN_15



// // Nuclear Analog
// // ADC3_INP5
// #define VMON_Pin 				GPIO_PIN_3
// #define VMON_GPIO_Port 			GPIOF


// // #define CMD_USART_TX_Pin 		GPIO_PIN_8
// // #define CMD_USART_TX_GPIO_Port 	GPIOD
// // #define CMD_USART_RX_Pin 		GPIO_PIN_9
// // #define CMD_USART_RX_GPIO_Port 	GPIOD
// #define CMD_USART_TX_Pin 		GPIO_PIN_9
// #define CMD_USART_TX_GPIO_Port 	GPIOB
// #define CMD_USART_RX_Pin 		GPIO_PIN_8
// #define CMD_USART_RX_GPIO_Port 	GPIOB

// #define SCK_Pin 				GPIO_PIN_2
// #define SCK_GPIO_Port 			GPIOE
// #define GP9_Pin 				GPIO_PIN_3
// #define GP9_GPIO_Port 			GPIOE
// #define CS_Pin 					GPIO_PIN_4
// #define CS_GPIO_Port 			GPIOE
// #define MISO_Pin 				GPIO_PIN_5
// #define MISO_GPIO_Port 			GPIOE
// #define MOSI_Pin 				GPIO_PIN_6
// #define MOSI_GPIO_Port 			GPIOE
// #define VMON_Pin 				GPIO_PIN_3
// #define VMON_GPIO_Port 			GPIOF
// #define GP8_Pin 				GPIO_PIN_5
// #define GP8_GPIO_Port 			GPIOF
// #define GP10_Pin 				GPIO_PIN_7
// #define GP10_GPIO_Port 			GPIOF
// #define GP0_Pin 				GPIO_PIN_0
// #define GP0_GPIO_Port 			GPIOC
// #define GP1_Pin 				GPIO_PIN_3
// #define GP1_GPIO_Port 			GPIOC
// #define GP7_Pin 				GPIO_PIN_3
// #define GP7_GPIO_Port 			GPIOA
// #define PIX_CLK_0_Pin 			GPIO_PIN_5
// #define PIX_CLK_0_GPIO_Port 	GPIOA
// #define PIX_DAT_0_Pin 			GPIO_PIN_7
// #define PIX_DAT_0_GPIO_Port 	GPIOA
// #define SPI1_MISO_Pin 			GPIO_PIN_6
// #define SPI1_MISO_GPIO_Port 	GPIOA
// #define SPI1_CS_Pin 			GPIO_PIN_4
// #define SPI1_CS_GPIO_Port 		GPIOA
// #define GP2_Pin 				GPIO_PIN_12
// #define GP2_GPIO_Port 			GPIOF
// #define GP3_Pin 				GPIO_PIN_13
// #define GP3_GPIO_Port 			GPIOF
// #define GP4_Pin 				GPIO_PIN_14
// #define GP4_GPIO_Port 			GPIOF
// #define LED1_Pin 				GPIO_PIN_15
// #define LED1_GPIO_Port 			GPIOF
// #define NC_Pin 					GPIO_PIN_1
// #define NC_GPIO_Port 			GPIOG
// #define WIFI_RXD_Pin 			GPIO_PIN_7
// #define WIFI_RXD_GPIO_Port 		GPIOE
// #define WIFI_TXD_Pin 			GPIO_PIN_8
// #define WIFI_TXD_GPIO_Port 		GPIOE
// #define GP5_Pin 				GPIO_PIN_9
// #define GP5_GPIO_Port 			GPIOE
// #define WIFI_RST_Pin 			GPIO_PIN_10
// #define WIFI_RST_GPIO_Port 		GPIOE
// #define NC2_Pin 				GPIO_PIN_11
// #define NC2_GPIO_Port 			GPIOE
// #define WIFI_PD_Pin 			GPIO_PIN_12
// #define WIFI_PD_GPIO_Port 		GPIOE
// #define GP6_Pin 				GPIO_PIN_13
// #define GP6_GPIO_Port 			GPIOE
// #define WIFI_RX_Ready_Pin 		GPIO_PIN_14
// #define WIFI_RX_Ready_Port 		GPIOE
// #define WIFI_BOOT_Pin 			GPIO_PIN_15
// #define WIFI_BOOT_GPIO_Port 	GPIOE
// #define WIFI_SS_Pin 			GPIO_PIN_11
// #define WIFI_SS_GPIO_Port 		GPIOB
// #define T0_Pin 					GPIO_PIN_14
// #define T0_GPIO_Port 			GPIOD
// #define T1_Pin 					GPIO_PIN_15
// #define T1_GPIO_Port 			GPIOD
// #define LED2_Pin 				GPIO_PIN_9
// #define LED2_GPIO_Port 			GPIOG
// #define LED0_Pin 				GPIO_PIN_14
// #define LED0_GPIO_Port 			GPIOG
// #define I2C1_SCL_UART4_RX_Pin 	GPIO_PIN_6
// #define I2C1_SCL_UART4_RX_GPIO_Port GPIOB
// #define I2C1_SDA_UART4_TX_Pin 		GPIO_PIN_7
// #define I2C1_SDA_UART4_TX_GPIO_Port GPIOB

#endif

#endif


