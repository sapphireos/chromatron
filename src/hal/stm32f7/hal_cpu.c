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

void SystemClock_Config(void);


void cpu_v_init( void ){

    SCB_EnableICache();
    SCB_EnableDCache();

    // HAL_Init();

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

void SystemClock_Config(void)
{
  // RCC_OscInitTypeDef RCC_OscInitStruct;
  // RCC_ClkInitTypeDef RCC_ClkInitStruct;

  //   /**Configure the main internal regulator output voltage 
  //   */
  // __HAL_RCC_PWR_CLK_ENABLE();
  // __HAL_RCC_SYSCFG_CLK_ENABLE();

  // __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  //   /**Initializes the CPU, AHB and APB busses clocks 
  //   */
  // RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  // RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  // RCC_OscInitStruct.HSICalibrationValue = 16;
  // RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  // RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  // RCC_OscInitStruct.PLL.PLLM = 16;
  // RCC_OscInitStruct.PLL.PLLN = 432;
  // RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  // RCC_OscInitStruct.PLL.PLLQ = 2;
  // if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  // {
  //   _Error_Handler(__FILE__, __LINE__);
  // }

  //   *Activate the Over-Drive mode 
    
  // if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  // {
  //   _Error_Handler(__FILE__, __LINE__);
  // }

  //   /**Initializes the CPU, AHB and APB busses clocks 
  //   */
  // RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
  //                             |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  // RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  // RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  // RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  // RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  // if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  // {
  //   _Error_Handler(__FILE__, __LINE__);
  // }

  //   /**Configure the Systick interrupt time 
  //   */
  // HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  //   /**Configure the Systick 
  //   */
  // HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_7)
  {
  Error_Handler();  
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

  LL_PWR_EnableOverDriveMode();

  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {
    
  }
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_16, 432, LL_RCC_PLLP_DIV_2);

  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {
    
  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);

  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  
  }
  
  LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);

  LL_RCC_SetUSARTClockSource(LL_RCC_USART6_CLKSOURCE_PCLK2);
}


// void SystemClock_Config(void)
// {

//   RCC_OscInitTypeDef RCC_OscInitStruct;
//   RCC_ClkInitTypeDef RCC_ClkInitStruct;

//     /**Configure the main internal regulator output voltage 
//     */
//   __HAL_RCC_PWR_CLK_ENABLE();

//   __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

//     /**Initializes the CPU, AHB and APB busses clocks 
//     */
//   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
//   RCC_OscInitStruct.HSIState = RCC_HSI_ON;
//   RCC_OscInitStruct.HSICalibrationValue = 16;
//   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
//   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
//   {
//     _Error_Handler(__FILE__, __LINE__);
//   }

//     /**Initializes the CPU, AHB and APB busses clocks 
//     */
//   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
//                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
//   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
//   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
//   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
//   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

//   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
//   {
//     _Error_Handler(__FILE__, __LINE__);
//   }

//     /**Configure the Systick interrupt time 
//     */
//   HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

//     /**Configure the Systick 
//     */
//   HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

//   /* SysTick_IRQn interrupt configuration */
//   HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
// }

// void SystemClock_Config(void)
// {

//   LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);

//   if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_7)
//   {
//   Error_Handler();  
//   }
//   LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

//   LL_PWR_EnableOverDriveMode();

//   LL_RCC_HSE_Enable();

//    /* Wait till HSE is ready */
//   while(LL_RCC_HSE_IsReady() != 1)
//   {
    
//   }
//   LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_16, 432, LL_RCC_PLLP_DIV_2);

//   LL_RCC_PLL_Enable();

//    /* Wait till PLL is ready */
//   while(LL_RCC_PLL_IsReady() != 1)
//   {
    
//   }
//   LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

//   LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);

//   LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);

//   LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

//    /* Wait till System clock is ready */
//   while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
//   {
  
//   }
//   LL_Init1msTick(216000000);

//   LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);

//   LL_SetSystemCoreClock(216000000);

//   LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);

//   LL_RCC_SetUSARTClockSource(LL_RCC_USART6_CLKSOURCE_PCLK2);

//   /* SysTick_IRQn interrupt configuration */
//   NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
// }

void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
