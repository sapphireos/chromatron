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



#include "cpu.h"

#include "system.h"
#include "config.h"

#include "aes.h"
#include "crypt.h"

// uncomment to use software AES on MAC function
//#define aes_v_encrypt aes_encrypt_128

/*

This module provides the following cryptographic primitives:

128 bit key generator (hardware RNG)
AES block cipher
AES-XCBC-MAC-96 (RFC 3566) - AES based MAC algorithm
AES-CBC (RFC 3602) - AES cipher algorithm (for encrypt/decrypt)


*/

// XOR a with b, placing result in a
static void xor_block( uint8_t *a, const uint8_t *b ){
   
   a[0] ^= b[0];
   a[1] ^= b[1];
   a[2] ^= b[2];
   a[3] ^= b[3];
   a[4] ^= b[4];
   a[5] ^= b[5];
   a[6] ^= b[6];
   a[7] ^= b[7];
   a[8] ^= b[8];
   a[9] ^= b[9];
   a[10] ^= b[10];
   a[11] ^= b[11];
   a[12] ^= b[12];
   a[13] ^= b[13];
   a[14] ^= b[14];
   a[15] ^= b[15];
}

/*
params:
k - Key
iv - Optional initialization vector.  This will be computed as the first block,
    and the rest of the data will be computed over m.
m - The message data to compute
n - Length of message data NOT including the IV.
auth - The output auth tag.

*/

void crypt_v_aes_xcbc_mac_96( 
    const uint8_t k[CRYPT_KEY_SIZE], 
    const uint8_t iv[CRYPT_KEY_SIZE], 
    const uint8_t *m, 
    uint16_t n, 
    uint8_t auth[CRYPT_AUTH_TAG_SIZE] ){
   
    uint8_t k1[CRYPT_KEY_SIZE];
    uint8_t k2[CRYPT_KEY_SIZE];
    uint8_t k3[CRYPT_KEY_SIZE];
    uint8_t o_key[CRYPT_KEY_SIZE];
    uint8_t temp[CRYPT_KEY_SIZE];
    uint8_t e[CRYPT_KEY_SIZE];

    // compute intermediate keys
    // RFC 3655 Step 1
    memset( temp, 1, sizeof(temp) );
    aes_v_encrypt( temp, k1, k, o_key ); 

    memset( temp, 2, sizeof(temp) );
    aes_v_encrypt( temp, k2, k, o_key ); 

    memset( temp, 3, sizeof(temp) );
    aes_v_encrypt( temp, k3, k, o_key ); 


    // RFC 3655 Step 2
    memset( e, 0, sizeof(e) );

    // compute number of blocks in message
    uint8_t blocks = 1;

    if( n > 16 ){
        
        blocks += ( ( n - 1 ) / 16 );
    }

    // check if IV is present
    if( iv != 0 ){

        // compute first block
        xor_block( e, iv );    
        aes_v_encrypt( e, e, k1, o_key );
    }

    // RFC 3655 Step 3
    for( uint8_t i = 0; i < ( blocks - 1 ); i++ ){
         
        xor_block( e, m );    
        aes_v_encrypt( e, e, k1, o_key );

        m += 16;
        n -= 16;
    }
    
    ASSERT( n <= 16 );

    // Final block step
    // RFC 3655 Step 4
    if( n == 16 ){
        
        xor_block( e, m );   
        xor_block( e, k2 );
        aes_v_encrypt( e, e, k1, o_key );
    }
    else{
       
        memset( temp, 0, sizeof(temp) );
        memcpy( temp, m, n );
        
        // pad
        temp[n] = 0x80;
        // all 0s remain
        
        xor_block( e, temp );   
        xor_block( e, k3 );
        aes_v_encrypt( e, e, k1, o_key );
    }
    
    memcpy( auth, e, CRYPT_AUTH_TAG_SIZE );
}

void crypt_v_aes_cbc_128_encrypt( uint8_t k[CRYPT_KEY_SIZE], uint8_t iv[CRYPT_KEY_SIZE], uint8_t *m, uint16_t n, uint8_t *o, uint16_t o_buf_size ){
    
    uint8_t o_key[16];
    uint8_t e[16];
   
    ASSERT( n > 0 );

    // compute number of blocks in message
    uint8_t blocks = 1;

    if( n > 16 ){
        
        blocks += ( ( n - 1 ) / 16 );
    }
   
    // check that output buffer is large enough
    ASSERT( o_buf_size >= (uint16_t)( ( blocks * 16 ) + 16 ) );

    // check if padding is needed
    if( n < (uint16_t)( blocks * 16 ) ){
        /*
        !!!!!!!!! THIS IS A POTENTIAL BUFFER OVERFLOW!!!!!!!!!!!!!!!!!!!
        */
        memset( m + n, 0, ( blocks * 16 ) - n );
    }

    // load IV
    memcpy( e, iv, sizeof(e) );

    // for blocks 0 to n
    // encrypt with CBC
    for( uint8_t i = 0; i < blocks; i++ ){
       
        xor_block( e, m );
        aes_v_encrypt( e, o, k, o_key ); 
        memcpy( e, o, sizeof(e) );

        o += 16;
        m += 16;
        n -= 16;
    }
}


void crypt_v_aes_cbc_128_decrypt( uint8_t k[CRYPT_KEY_SIZE], uint8_t iv[CRYPT_KEY_SIZE], uint8_t *m, uint16_t n, uint8_t *o, uint16_t o_buf_size ){
    
    uint8_t o_key[16];
    uint8_t e[16];
   
    ASSERT( n > 0 );

    // compute number of blocks in message
    uint8_t blocks = 1;

    if( n > 16 ){
        
        blocks += ( ( n - 1 ) / 16 );
    }
   
    // check that output buffer is large enough
    ASSERT( o_buf_size >= (uint16_t)( ( blocks * 16 ) + 16 ) );

    // check if padding is needed
    /*if( n < ( blocks * 16 ) ){
        
        memset( m + n, 0, ( blocks * 16 ) - n );
    }*/

    // load IV
    memcpy( e, iv, sizeof(e) );

    // for blocks 0 to n
    // encrypt with CBC
    for( uint8_t i = 0; i < blocks; i++ ){
       
        aes_v_decrypt( m, o, k, o_key ); 
        xor_block( o, e );
        memcpy( e, m, sizeof(e) );

        o += 16;
        m += 16;
        n -= 16;
    }
}
