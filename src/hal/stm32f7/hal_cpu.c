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


#include "hal_cpu.h"
#include "cpu.h"

#include "system.h"

void SystemClock_Config(void);


void cpu_v_init( void ){

    SCB_EnableICache();
    SCB_EnableDCache();


    __HAL_FLASH_ART_ENABLE();

    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();

    /* Set Interrupt Group Priority */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* Use systick as time base source and configure 1ms tick (default clock after Reset is HSI) */
    HAL_InitTick(TICK_INT_PRIORITY);


    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* System interrupt init*/
    /* MemoryManagement_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
    /* BusFault_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
    /* UsageFault_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
    /* SVCall_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
    /* DebugMonitor_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
    /* PendSV_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);


    // update clock
    SystemClock_Config();

    // update clock frequency info
    SystemCoreClockUpdate();

    // enable SYSTICK
    LL_Init1msTick(cpu_u32_get_clock_speed());
    LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);

    // enable SYSCFG controller clock
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // enable gpio clocks
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);

    trace_printf("CPU Clock: %u\n", cpu_u32_get_clock_speed());
    trace_printf("HCLK     : %u\n", HAL_RCC_GetHCLKFreq());
    trace_printf("PCLK1    : %u\n", HAL_RCC_GetPCLK1Freq());
    trace_printf("PCLK2    : %u\n", HAL_RCC_GetPCLK2Freq());
}

uint8_t cpu_u8_get_reset_source( void ){

    if( LL_RCC_IsActiveFlag_SFTRST() ){

        return 0;
    }

    if( LL_RCC_IsActiveFlag_PORRST() ){

        return RESET_SOURCE_POWER_ON;  
    }

    if( LL_RCC_IsActiveFlag_BORRST() ){

        return RESET_SOURCE_BROWNOUT;
    }

    if( LL_RCC_IsActiveFlag_PINRST() ){

        return RESET_SOURCE_EXTERNAL;
    }

    return 0;
}

void cpu_v_clear_reset_source( void ){

    LL_RCC_ClearResetFlags();    
}

void cpu_v_remap_isrs( void ){

}

void cpu_v_sleep( void ){

}

bool cpu_b_osc_fail( void ){

    return 0;
}

uint32_t cpu_u32_get_clock_speed( void ){

    return SystemCoreClock;
}

void cpu_reboot( void ){

    HAL_NVIC_SystemReset();    
}

void SystemClock_Config( void ){
  
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);

    if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_7){

        Error_Handler();  
    }

    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

    LL_PWR_EnableOverDriveMode();

    LL_RCC_HSE_Enable();

    /* Wait till HSE is ready */
    while(LL_RCC_HSE_IsReady() != 1);

    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_16, 432, LL_RCC_PLLP_DIV_2);

    LL_RCC_PLL_Enable();

    /* Wait till PLL is ready */
    while(LL_RCC_PLL_IsReady() != 1);
    
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);

    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);

    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

    /* Wait till System clock is ready */
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);

    LL_RCC_SetUSARTClockSource(LL_RCC_USART6_CLKSOURCE_PCLK2);
}


void _Error_Handler( char *file, int line ){
  
    assert( 0, file, line );
}
