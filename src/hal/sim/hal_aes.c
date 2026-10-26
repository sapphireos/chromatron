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

#include "aes.h"

#include <string.h>

/*

AES (sim)

There is no AES implementation in the workspace for the host toolchain:
these stubs satisfy the linker. They zero the output and must not be
used for real cryptography in the simulator.

*/

void aes_v_set_key( const uint8_t key[AES_BLOCK_SIZE] ){

}

void aes_v_encrypt( const uint8_t in[AES_BLOCK_SIZE],
                    uint8_t out[AES_BLOCK_SIZE],
                    const uint8_t key[AES_BLOCK_SIZE],
                    uint8_t o_key[AES_BLOCK_SIZE] ){

    memset( out, 0, AES_BLOCK_SIZE );
    memset( o_key, 0, AES_BLOCK_SIZE );
}

void aes_v_decrypt( const uint8_t in[AES_BLOCK_SIZE],
                    uint8_t out[AES_BLOCK_SIZE],
                    const uint8_t key[AES_BLOCK_SIZE],
                    uint8_t o_key[AES_BLOCK_SIZE] ){

    memset( out, 0, AES_BLOCK_SIZE );
    memset( o_key, 0, AES_BLOCK_SIZE );
}
