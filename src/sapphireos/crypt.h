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


#ifndef _CRYPT_H
#define _CRYPT_H

#define CRYPT_KEY_SIZE          16
#define CRYPT_AUTH_TAG_SIZE     12


void crypt_v_aes_xcbc_mac_96( 
    const uint8_t k[CRYPT_KEY_SIZE], 
    const uint8_t iv[CRYPT_KEY_SIZE], 
    const uint8_t *m, 
    uint16_t n, 
    uint8_t auth[CRYPT_AUTH_TAG_SIZE] );
void crypt_v_aes_cbc_128_encrypt( uint8_t k[CRYPT_KEY_SIZE], uint8_t iv[CRYPT_KEY_SIZE], uint8_t *m, uint16_t n, uint8_t *o, uint16_t o_buf_size );
void crypt_v_aes_cbc_128_decrypt( uint8_t k[CRYPT_KEY_SIZE], uint8_t iv[CRYPT_KEY_SIZE], uint8_t *m, uint16_t n, uint8_t *o, uint16_t o_buf_size );










#endif

