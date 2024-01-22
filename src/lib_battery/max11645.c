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


#include "sapphire.h"

#ifdef ENABLE_PATCH_BOARD

#include "max11645.h"


/*

12 bits, 2 channels

Default configuration at power up:

Setup byte:
Reference voltage: VDD
Internal clock
Unipolar (positive voltage)

Config byte:
Scan from AIN0 to CS0
channel select ch 0
Single ended


*/

void max11645_v_init( void ){

	// reset configuration
	uint8_t cmd[2];
    cmd[0] = 0b10000010; // setup byte
    cmd[1] = 0b00000001; // config byte

    i2c_v_write( MAX11645_I2C_ADDR, cmd, sizeof(cmd) );
}


uint16_t max11645_u16_read( uint8_t channel ){

	if( channel >= MAX11645_N_CHANNELS ){

		return 0;
	}

	// set channel
	uint8_t cfg = MAX11645_BIT_CONFIG | MAX11645_BIT_CFG_SE | MAX11645_BIT_CFG_SCAN0 | MAX11645_BIT_CFG_SCAN1; // scan CS0

	if( channel == 1 ){

		cfg |= MAX11645_BIT_CFG_CS;
	}
    

    i2c_v_write( MAX11645_I2C_ADDR, &cfg, sizeof(cfg) );


    uint8_t data[2] = {0};
    i2c_v_read( MAX11645_I2C_ADDR, (uint8_t *)data, sizeof(data) );

    return ( (uint16_t)( data[0] & 0x0F ) << 8 ) | data[1];
}

#endif