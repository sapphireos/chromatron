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
#include "onewire.h"

#if defined(ESP32)

#include "hal_onewire.h"

static const uint8_t crc_table[256] = {
    0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
    157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
    35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
    190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
    70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
    219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
    101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
    248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
    140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
    17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
    175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
    50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
    202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
    87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
    233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
    116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
};

void onewire_v_init( uint8_t _io_pin ){

    hal_onewire_v_init( _io_pin );                
}


void onewire_v_deinit( void ){

    hal_onewire_v_deinit();                
}

uint8_t onewire_u8_crc( uint8_t *data, uint8_t len ){

    uint8_t crc = 0;

    while( len > 0 ){

        len--;

        crc = crc_table[crc ^ *data];

        data++;
    }

    return crc;
}

// return TRUE if a device is present
bool onewire_b_reset( void ){

    return hal_onewire_b_reset();
}

void onewire_v_write_byte( uint8_t byte ){

    hal_onewire_v_write_byte( byte );
}

uint8_t onewire_u8_read_byte( void ){

    return hal_onewire_u8_read_byte();
}

// returns TRUE if valid
bool onewire_b_read_rom_id( uint8_t *family_code, uint64_t *id ){

    *family_code = 0;
    *id = 0;
    
    onewire_v_write_byte( ONEWIRE_CMD_READ_ROM_ID );

    uint8_t data[8] = {0};

    for( uint8_t i = 0; i < 8; i++ ){

        data[i] = onewire_u8_read_byte();
    }    

    uint8_t crc = onewire_u8_crc( data, sizeof(data) - 1 );

    // check if crc is correct
    if( crc != data[sizeof(data) - 1] ){

        // nope
        return FALSE;
    }

    *family_code = data[0];
    memcpy( id, &data[1], ONEWIRE_ID_LEN );

    return TRUE;
}

#else

void onewire_v_init( void ){

}

#endif


