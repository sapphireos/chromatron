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

#define Poly16 0x1021



uint16_t crc_u16_start( void ){

    // reset CRC to 0xFFFF and IO interface
    CRC.CTRL = CRC_RESET_RESET1_gc;
    asm volatile("nop"); // nop to allow one cycle delay for CRC to reset.

    // change initial value to 0x1D0F, as this is what our existing
    // tools are using.
    CRC.CHECKSUM1 = 0x1D;
    CRC.CHECKSUM0 = 0x0F;
    CRC.CTRL |= CRC_SOURCE_IO_gc;

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

    while( length > 0 ){

        CRC.DATAIN = *ptr;
        ptr++;
        length--;
    }

    return ( (uint16_t)CRC.CHECKSUM1 << 8 ) | CRC.CHECKSUM0;
}

uint16_t crc_u16_byte( uint16_t crc, uint8_t data ){

    CRC.DATAIN = data;

    return ( (uint16_t)CRC.CHECKSUM1 << 8 ) | CRC.CHECKSUM0;
}

uint16_t crc_u16_finish( uint16_t crc ){

    CRC.STATUS = CRC_BUSY_bm;

    return ( (uint16_t)CRC.CHECKSUM1 << 8 ) | CRC.CHECKSUM0;
}

void crc_v_init( void ){

}
