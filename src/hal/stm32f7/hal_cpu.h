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


#ifndef _HAL_CPU_H	
#define _HAL_CPU_H

#include "bool.h"

#include "stm32f7xx.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_system.h"
#include "stm32f7xx_ll_exti.h"
#include "stm32f7xx_ll_cortex.h"
#include "stm32f7xx_ll_utils.h"
#include "stm32f7xx_ll_pwr.h"
#include "stm32f7xx_ll_dma.h"
#include "stm32f7xx_ll_spi.h"
#include "stm32f7xx_ll_usart.h"
#include "stm32f7xx_ll_gpio.h"

#include "Trace.h"


#define ENABLE_INTERRUPTS 
#define DISABLE_INTERRUPTS 

#define RESET_SOURCE_POWER_ON   0x01
#define RESET_SOURCE_JTAG       0x02
#define RESET_SOURCE_EXTERNAL   0x04
#define RESET_SOURCE_BROWNOUT   0x08

#define ATOMIC
#define END_ATOMIC

#define FLASH_STRING(x) x
#define FLASH_STRING_T const char*

#define FW_INFO_SECTION __attribute__ ((section (".fwinfo"), used))


#endif
