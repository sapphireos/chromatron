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


#include "hal_cpu.h"
#include "cpu.h"
#include "hal_timers.h"

#include "system.h"

#ifdef BOARD_CHROMATRONX
#pragma message "BOARD_CHROMATRONX"
#endif


static void cpu_normal_clock_config( void ){

    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /**Supply configuration update enable 
    */
    MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);

    /**Configure the main internal regulator output voltage 
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /*
    Voltage scaling max frequencies from data sheet:
    1 - 400 MHz
    2 - 300 MHz
    3 - 200 MHz (startup default)
    */

    while ((PWR->D3CR & (PWR_D3CR_VOSRDY)) != PWR_D3CR_VOSRDY) 
    {

    }
    /**Initializes the CPU, AHB and APB busses clocks 
    */
    // PLLs sourced to HSE (external xtal) - requires 16 MHz xtal
    // note that chromatron X has an 8 MHz crystal (from the nucleo boards)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    // set PLL1 to 400 MHz
    #ifdef BOARD_CHROMATRONX
    RCC_OscInitStruct.PLL.PLLM = 1;
    #else
    RCC_OscInitStruct.PLL.PLLM = 2;
    #endif
    RCC_OscInitStruct.PLL.PLLN = 100;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV4;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    // set flash latency
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    // set up peripheral clocks
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_UART4|RCC_PERIPHCLK_UART7
                              |RCC_PERIPHCLK_USART6|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_UART8|RCC_PERIPHCLK_UART5
                              |RCC_PERIPHCLK_RNG|RCC_PERIPHCLK_SPI5
                              |RCC_PERIPHCLK_SPI4|RCC_PERIPHCLK_SPI3
                              |RCC_PERIPHCLK_SPI1|RCC_PERIPHCLK_SPI2
                              |RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_SPI6|RCC_PERIPHCLK_QSPI|
                              RCC_PERIPHCLK_USB;

    // set PLL 2 to 100 MHz
    #ifdef BOARD_CHROMATRONX
    PeriphClkInitStruct.PLL2.PLL2M = 1;
    #else
    PeriphClkInitStruct.PLL2.PLL2M = 2;
    #endif
    PeriphClkInitStruct.PLL2.PLL2N = 25;
    PeriphClkInitStruct.PLL2.PLL2P = 2;
    PeriphClkInitStruct.PLL2.PLL2Q = 2;
    PeriphClkInitStruct.PLL2.PLL2R = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;

    // set PLL 3 to 64 MHz
    #ifdef BOARD_CHROMATRONX
    PeriphClkInitStruct.PLL3.PLL3M = 8;
    #else
    PeriphClkInitStruct.PLL3.PLL3M = 16;
    #endif
    PeriphClkInitStruct.PLL3.PLL3N = 256;
    PeriphClkInitStruct.PLL3.PLL3P = 4;
    PeriphClkInitStruct.PLL3.PLL3Q = 4;
    PeriphClkInitStruct.PLL3.PLL3R = 4;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;    

    // connect peripheral clocks
    PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
    PeriphClkInitStruct.Spi45ClockSelection = RCC_SPI45CLKSOURCE_PLL2;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL3;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_PLL3;
    PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
    PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_PLL3;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;
    PeriphClkInitStruct.Spi6ClockSelection = RCC_SPI6CLKSOURCE_PLL2;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /**Configure the Systick interrupt time 
    */
    HAL_SYSTICK_Config(SystemCoreClock/1000);

    /**Configure the Systick 
    */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

    // update clock frequency info
    SystemCoreClockUpdate();
}

static void cpu_boot_clock_config( void ){

    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /**Supply configuration update enable 
    */
    MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);

    /**Configure the main internal regulator output voltage 
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    while ((PWR->D3CR & (PWR_D3CR_VOSRDY)) != PWR_D3CR_VOSRDY) 
    {

    }
    /**Initializes the CPU, AHB and APB busses clocks 
    */
    // PLLs sourced to HSI (64 MHz)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    // set PLL1 to 80 MHz
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV4;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    // set flash latency
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }

    // set up peripheral clocks
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_UART4|RCC_PERIPHCLK_UART7
                              |RCC_PERIPHCLK_USART6|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_UART8|RCC_PERIPHCLK_UART5
                              |RCC_PERIPHCLK_RNG|RCC_PERIPHCLK_SPI5
                              |RCC_PERIPHCLK_SPI4|RCC_PERIPHCLK_SPI3
                              |RCC_PERIPHCLK_SPI1|RCC_PERIPHCLK_SPI2
                              |RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_SPI6|RCC_PERIPHCLK_QSPI;

    // set PLL 2 to 32 MHz
    PeriphClkInitStruct.PLL2.PLL2M = 8;
    PeriphClkInitStruct.PLL2.PLL2N = 8;
    PeriphClkInitStruct.PLL2.PLL2P = 2;
    PeriphClkInitStruct.PLL2.PLL2Q = 2;
    PeriphClkInitStruct.PLL2.PLL2R = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;

    // set PLL 3 to 32 MHz
    PeriphClkInitStruct.PLL3.PLL3M = 8;
    PeriphClkInitStruct.PLL3.PLL3N = 8;
    PeriphClkInitStruct.PLL3.PLL3P = 4;
    PeriphClkInitStruct.PLL3.PLL3Q = 4;
    PeriphClkInitStruct.PLL3.PLL3R = 4;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;    

    // connect peripheral clocks
    PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
    PeriphClkInitStruct.Spi45ClockSelection = RCC_SPI45CLKSOURCE_PLL2;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL3;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_PLL3;
    PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
    PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_PLL3;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;
    PeriphClkInitStruct.Spi6ClockSelection = RCC_SPI6CLKSOURCE_PLL2;
  
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    // update clock frequency info
    SystemCoreClockUpdate();
}

static void cpu_init_noncacheable( void ){
    
    // set up MPU and initialize non-cacheable RAM section
    HAL_MPU_Disable();

    extern uint8_t _snon_cacheable, _enon_cacheable;

    // // clear non-cacheable region
    memset( &_snon_cacheable, 0, &_enon_cacheable - &_snon_cacheable );

    // // NOTE
    // // this does set for non-cacheable.  The S, C, and B bits don't mean what the say
    // // on the Cortex.  
    // //
    // // With Type Ext 0:
    // // Clearing B (bufferable) is what changes to write-through policy (but reads are still cached!)
    // // Clearing C will cause hard faults.
    // //
    // // With Type Ext 1:
    // // Clear B and C to disable caching alltogether.
    // // This is what we want for a true non-cacheable region, so no writes or reads will be cached.

    MPU_Region_InitTypeDef mpu_init;
    mpu_init.Enable             = MPU_REGION_ENABLE;
    mpu_init.BaseAddress        = (uint32_t)&_snon_cacheable;
    mpu_init.Size               = MPU_REGION_SIZE_32KB;
    mpu_init.AccessPermission   = MPU_REGION_FULL_ACCESS;
    mpu_init.IsBufferable       = MPU_ACCESS_NOT_BUFFERABLE;
    mpu_init.IsCacheable        = MPU_ACCESS_NOT_CACHEABLE;
    mpu_init.IsShareable        = MPU_ACCESS_NOT_SHAREABLE;
    mpu_init.Number             = MPU_REGION_NUMBER0;
    mpu_init.TypeExtField       = MPU_TEX_LEVEL1;
    mpu_init.SubRegionDisable   = 0x00;
    mpu_init.DisableExec        = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&mpu_init);
    
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}   


void cpu_v_init( void ){

    DISABLE_INTERRUPTS;

    SCB_EnableICache();
    SCB_EnableDCache();

    HAL_MPU_Disable();

    SCB->VTOR = FLASH_START;

    /* Set Interrupt Group Priority */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* Use systick as time base source and configure 1ms tick (default clock after Reset is HSI) */
    HAL_InitTick(TICK_INT_PRIORITY);

    // enable SYSCFG controller clock
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
    cpu_normal_clock_config();


    cpu_init_noncacheable();

    // enable gpio clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    
    PLL1_ClocksTypeDef pll1_clk;
    PLL2_ClocksTypeDef pll2_clk;
    PLL3_ClocksTypeDef pll3_clk;
    HAL_RCCEx_GetPLL1ClockFreq( &pll1_clk );
    HAL_RCCEx_GetPLL2ClockFreq( &pll2_clk );
    HAL_RCCEx_GetPLL3ClockFreq( &pll3_clk );

    uint16_t revision = HAL_GetREVID();

    switch( revision ){
        case STM32H7_REV_Z:
            trace_printf( "STM32H7 rev Z\r\n" );
            break;

        case STM32H7_REV_Y:
            trace_printf( "STM32H7 rev Y\r\n" );
            break;

        case STM32H7_REV_X:
            trace_printf( "STM32H7 rev X\r\n" );
            break;

        case STM32H7_REV_V:
            trace_printf( "STM32H7 rev V\r\n" );
            break;

        default:
            trace_printf( "STM32H7 rev UNKNOWN\r\n" );
            break;
    }

    trace_printf( "CPU Clock: %u\r\n", cpu_u32_get_clock_speed() );
    trace_printf( "HCLK     : %u\r\n", HAL_RCC_GetHCLKFreq() );
    trace_printf( "PCLK1    : %u\r\n", HAL_RCC_GetPCLK1Freq() );
    trace_printf( "PCLK2    : %u\r\n", HAL_RCC_GetPCLK2Freq() );

    trace_printf( "D1Sys    : %u\r\n", HAL_RCCEx_GetD1SysClockFreq() );

    trace_printf( "PLL1 P   : %u\r\n", pll1_clk.PLL1_P_Frequency );
    // trace_printf( "PLL1 Q   : %u\r\n", pll1_clk.PLL1_Q_Frequency );
    // trace_printf( "PLL1 R   : %u\r\n", pll1_clk.PLL1_R_Frequency );
    trace_printf( "PLL2 P   : %u\r\n", pll2_clk.PLL2_P_Frequency );
    // trace_printf( "PLL2 Q   : %u\r\n", pll2_clk.PLL2_Q_Frequency );
    // trace_printf( "PLL2 R   : %u\r\n", pll2_clk.PLL2_R_Frequency );
    trace_printf( "PLL3 P   : %u\r\n", pll3_clk.PLL3_P_Frequency );
    // trace_printf( "PLL3 Q   : %u\r\n", pll3_clk.PLL3_Q_Frequency );
    // trace_printf( "PLL3 R   : %u\r\n", pll3_clk.PLL3_R_Frequency );

    hal_timer_v_preinit();
}

uint8_t cpu_u8_get_reset_source( void ){

    if( __HAL_RCC_GET_FLAG( RCC_FLAG_SFTRST ) ){

        return 0;
    }

    if( __HAL_RCC_GET_FLAG( RCC_FLAG_PORRST ) ){

        return RESET_SOURCE_POWER_ON;  
    }

    if( __HAL_RCC_GET_FLAG( RCC_FLAG_BORRST ) ){

        return RESET_SOURCE_BROWNOUT;
    }

    if( __HAL_RCC_GET_FLAG( RCC_FLAG_PINRST ) ){

        return RESET_SOURCE_EXTERNAL;
    }

    return 0;
}

void cpu_v_clear_reset_source( void ){

    __HAL_RCC_CLEAR_RESET_FLAGS();    
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

int *__errno( void ){
  return &_REENT->_errno;
}


void _Error_Handler( char *file, int line ){
  
    assert( 0, file, line );
}
    
#pragma GCC push_options
#pragma GCC optimize ("O3")
void hal_cpu_v_delay_us( uint16_t us ){

    // enable trace unit
    // this will already be on when connecting with the debugger,
    // but defaults to off if not debugger is attached.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // enable cycle counter

    volatile uint32_t target_count = ( cpu_u32_get_clock_speed() / 1000000 ) * (uint32_t)us;
    volatile uint32_t start_cycles = DWT->CYCCNT;
    
    while( ( DWT->CYCCNT - start_cycles ) < target_count );
}
#pragma GCC pop_options



void hal_cpu_v_boot_init( void ){

    DISABLE_INTERRUPTS;

    // note caches must be enabled, or the cpu will fail to start up unless you
    // hit reset several times.  no idea why this is happening, but
    // we don't have a good reason to turn off the caches anyway.
    SCB_EnableICache();
    SCB_EnableDCache();

    HAL_MPU_Disable();


    SCB->VTOR = BOOTLOADER_FLASH_START;

    /* Set Interrupt Group Priority */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);


    // enable SYSCFG controller clock
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
    
    // update clock
    cpu_boot_clock_config();

    /* Use systick as time base source and configure 1ms tick (default clock after Reset is HSI) */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
    HAL_InitTick(TICK_INT_PRIORITY);
    
    // enable gpio clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    PLL1_ClocksTypeDef pll1_clk;
    PLL2_ClocksTypeDef pll2_clk;
    PLL3_ClocksTypeDef pll3_clk;
    HAL_RCCEx_GetPLL1ClockFreq( &pll1_clk );
    HAL_RCCEx_GetPLL2ClockFreq( &pll2_clk );
    HAL_RCCEx_GetPLL3ClockFreq( &pll3_clk );

    trace_printf( "STM32H7 Bootloader\r\n" );

    trace_printf( "CPU Clock: %u\r\n", cpu_u32_get_clock_speed() );
    trace_printf( "HCLK     : %u\r\n", HAL_RCC_GetHCLKFreq() );
    trace_printf( "PCLK1    : %u\r\n", HAL_RCC_GetPCLK1Freq() );
    trace_printf( "PCLK2    : %u\r\n", HAL_RCC_GetPCLK2Freq() );

    trace_printf( "D1Sys    : %u\r\n", HAL_RCCEx_GetD1SysClockFreq() );

    trace_printf( "PLL1 P   : %u\r\n", pll1_clk.PLL1_P_Frequency );
    trace_printf( "PLL1 Q   : %u\r\n", pll1_clk.PLL1_Q_Frequency );
    trace_printf( "PLL1 R   : %u\r\n", pll1_clk.PLL1_R_Frequency );
    trace_printf( "PLL2 P   : %u\r\n", pll2_clk.PLL2_P_Frequency );
    trace_printf( "PLL2 Q   : %u\r\n", pll2_clk.PLL2_Q_Frequency );
    trace_printf( "PLL2 R   : %u\r\n", pll2_clk.PLL2_R_Frequency );
    trace_printf( "PLL3 P   : %u\r\n", pll3_clk.PLL3_P_Frequency );
    trace_printf( "PLL3 Q   : %u\r\n", pll3_clk.PLL3_Q_Frequency );
    trace_printf( "PLL3 R   : %u\r\n", pll3_clk.PLL3_R_Frequency );
}
