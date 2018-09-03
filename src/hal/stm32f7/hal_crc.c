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

#include "stm32f7xx_ll_crc.h"


uint16_t crc_u16_start( void ){

    LL_CRC_ResetCRCCalculationUnit( CRC );

    return 0xffff;
}


uint16_t crc_u16_block( uint8_t *ptr, uint16_t length ){

    crc_u16_start();

    crc_u16_partial_block( 0xffff, ptr, length );

    return crc_u16_finish( 0 );
}

// compute the CRC for the given block with initialization or
// finishing the last two 0 bytes
uint16_t crc_u16_partial_block( uint16_t crc, uint8_t *ptr, uint16_t length ){

    while( length > 0 ){

        LL_CRC_FeedData8( CRC, *ptr );

        ptr++;
        length--;
    }

    return LL_CRC_ReadData16( CRC );
}

uint16_t crc_u16_byte( uint16_t crc, uint8_t data ){

    LL_CRC_FeedData8( CRC, data );

    return LL_CRC_ReadData16( CRC );
}

uint16_t crc_u16_finish( uint16_t crc ){
    
    return LL_CRC_ReadData16( CRC );
}

void crc_v_init( void ){

    // enable clock
    LL_AHB1_GRP1_EnableClock( LL_AHB1_GRP1_PERIPH_CRC );

    LL_CRC_SetPolynomialSize( CRC, LL_CRC_POLYLENGTH_16B );
    LL_CRC_SetInputDataReverseMode( CRC, LL_CRC_INDATA_REVERSE_NONE );
    LL_CRC_SetOutputDataReverseMode( CRC, LL_CRC_OUTDATA_REVERSE_NONE );
    LL_CRC_SetInitialData( CRC, 0x1D0F );
    LL_CRC_SetPolynomialCoef( CRC, 0x1021 );


}
