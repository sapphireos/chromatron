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


/********************************************************
						CRC16

	This program implements the CRC-CCITT:

	Polynomial = 0x1021
	Initial Value = 0x1D0F
	No reflection on input or output

********************************************************/


#include "cpu.h"

#include "target.h"
#include "crc.h"

static CRC_HandleTypeDef hcrc;

uint16_t crc_u16_start( void ){

    __HAL_CRC_DR_RESET( &hcrc );

    return 0xffff;
}


uint16_t crc_u16_block( uint8_t *ptr, uint32_t length ){

    crc_u16_start();

    crc_u16_partial_block( 0xffff, ptr, length );

    return crc_u16_finish( 0 );
}

// compute the CRC for the given block with initialization or
// finishing the last two 0 bytes
uint16_t crc_u16_partial_block( uint16_t crc, uint8_t *ptr, uint32_t length ){

    return HAL_CRC_Accumulate( &hcrc, (uint32_t *)ptr, length );
}

uint16_t crc_u16_byte( uint16_t crc, uint8_t data ){

    return HAL_CRC_Accumulate( &hcrc, (uint32_t *)&data, 1 );
}

uint16_t crc_u16_finish( uint16_t crc ){
    
    return hcrc.Instance->DR;
}

void crc_v_init( void ){

    // enable clock
    __HAL_RCC_CRC_CLK_ENABLE();

    // init CRC module
    hcrc.Instance = CRC;
    hcrc.Init.DefaultPolynomialUse      = DEFAULT_POLYNOMIAL_DISABLE;
    hcrc.Init.DefaultInitValueUse       = DEFAULT_INIT_VALUE_DISABLE;
    hcrc.Init.InputDataInversionMode    = CRC_INPUTDATA_INVERSION_NONE;
    hcrc.Init.OutputDataInversionMode   = CRC_OUTPUTDATA_INVERSION_DISABLE;
    hcrc.Init.CRCLength                 = CRC_POLYLENGTH_16B;
    hcrc.Init.GeneratingPolynomial      = 0x1021;
    hcrc.Init.InitValue                 = 0x1D0F;
    hcrc.InputDataFormat                = CRC_INPUTDATA_FORMAT_BYTES;

    if (HAL_CRC_Init(&hcrc) != HAL_OK)
    {
        _Error_Handler(__FILE__, __LINE__);
    }
}
