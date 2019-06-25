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

#include "hal_io.h"

#include "hal_i2c.h"

#include "i2c.h"



/*

STM32H7

I2C1
SCL - PB8
SDA - PB9

*/
typedef struct{
    uint32_t pin;
    uint32_t alt;
} hal_i2c_ch_t;


#ifdef BOARD_CHROMATRONX
static const hal_i2c_ch_t i2c_io[] = {
    { IO_PIN_GPIOSCL, GPIO_AF4_I2C1 },
    { IO_PIN_GPIOSDA, GPIO_AF4_I2C1 },
};
#else
static const hal_i2c_ch_t i2c_io[] = {
    { IO_PIN_GPIOSCL, GPIO_AF4_I2C1 },
    { IO_PIN_GPIOSDA, GPIO_AF4_I2C1 },
};
#endif

static I2C_HandleTypeDef i2c1;

void i2c_v_init( i2c_baud_t8 baud ){

	__HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_TypeDef *port;
	GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    hal_io_v_get_port( i2c_io[0].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = i2c_io[0].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );

    hal_io_v_get_port( i2c_io[1].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = i2c_io[1].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );


    uint32_t timing = 0;
    switch( baud ){
    	case I2C_BAUD_100K:
    		timing = 0x10C0ECFF;
    		break;

    	case I2C_BAUD_200K:
    		timing = 0x0090E5FF;
    		break;

    	case I2C_BAUD_300K:
    		timing = 0x00903EFF;
    		break;

    	case I2C_BAUD_400K:
    		timing = 0x009034B6;
    		break;

    	case I2C_BAUD_1000K:
    		timing = 0x00403EFF;
    		break;

    	default:
    		ASSERT( FALSE );
    		break;
    }

    I2C_InitTypeDef init;
    init.Timing 			= timing;
    init.OwnAddress1 		= 0;
    init.AddressingMode 	= I2C_ADDRESSINGMODE_7BIT;
    init.DualAddressMode 	= I2C_DUALADDRESS_DISABLE;
    init.OwnAddress2 		= 0;
    init.OwnAddress2Masks 	= I2C_OA2_NOMASK;
    init.GeneralCallMode 	= I2C_GENERALCALL_DISABLE;
    init.NoStretchMode 		= I2C_NOSTRETCH_DISABLE;

    i2c1.Instance = I2C1;
    i2c1.Init = init;

    HAL_I2C_Init( &i2c1 );
}

void i2c_v_set_pins( uint8_t clock, uint8_t data ){
	// no-op on this platform
}

uint8_t i2c_u8_status( void ){

    return 0;
}

void i2c_v_write( uint8_t dev_addr, const uint8_t *src, uint8_t len ){

	HAL_I2C_Master_Transmit( &i2c1, dev_addr, (uint8_t *)src, len, I2C_TIMEOUT );
}

void i2c_v_read( uint8_t dev_addr, uint8_t *dst, uint8_t len ){

	HAL_I2C_Master_Receive( &i2c1, dev_addr, dst, len, I2C_TIMEOUT );
}

void i2c_v_mem_write( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, const uint8_t *src, uint8_t len, uint16_t delay_ms ){

    // delay_ms is ignored in this driver.  some I2C devices that rely on it may fail!

	HAL_I2C_Mem_Write( &i2c1, dev_addr, mem_addr, addr_size, (uint8_t *)src, len, I2C_TIMEOUT );
}

void i2c_v_mem_read( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, uint8_t *dst, uint8_t len, uint16_t delay_ms ){

    // delay_ms is ignored in this driver.  some I2C devices that rely on it may fail!

	HAL_I2C_Mem_Read( &i2c1, dev_addr, mem_addr, addr_size, dst, len, I2C_TIMEOUT );	
}

