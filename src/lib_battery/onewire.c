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

// #include "hal_onewire.h"

static uint8_t io_pin;

void onewire_v_init( uint8_t _io_pin ){

    io_pin = _io_pin;

    io_v_set_mode( io_pin, IO_MODE_INPUT );




    // hal_onewire_v_init();                
}

static void delay_a( void ){

    _delay_us( ONEWIRE_DELAY_A );
}

static void delay_b( void ){

    _delay_us( ONEWIRE_DELAY_B );
}

static void delay_c( void ){

    _delay_us( ONEWIRE_DELAY_C );
}

static void delay_d( void ){

    _delay_us( ONEWIRE_DELAY_D );
}

static void delay_e( void ){

    _delay_us( ONEWIRE_DELAY_E );
}

static void delay_f( void ){

    _delay_us( ONEWIRE_DELAY_F );
}

static void delay_g( void ){

    _delay_us( ONEWIRE_DELAY_G );
}

static void delay_h( void ){

    _delay_us( ONEWIRE_DELAY_H );
}

static void delay_i( void ){

    _delay_us( ONEWIRE_DELAY_I );
}

static void delay_j( void ){

    _delay_us( ONEWIRE_DELAY_J );
}

static void drive_low( void ){

    io_v_set_mode( io_pin, IO_MODE_OUTPUT_OPEN_DRAIN );
    io_v_digital_write( io_pin, 0 );
}

static void drive_high( void ){

    io_v_set_mode( io_pin, IO_MODE_OUTPUT_OPEN_DRAIN );
    io_v_digital_write( io_pin, 1 );
}

static void release( void ){

    io_v_set_mode( io_pin, IO_MODE_INPUT );
}

static bool sample( void ){

    return io_b_digital_read( io_pin );
}


// return TRUE if a device is present
bool onewire_b_reset( void ){

    ATOMIC;

    delay_g();
    drive_low();
    delay_h();
    release();
    delay_i();
    bool bit = sample();
    delay_j();

    END_ATOMIC;

    return !bit;
}

void onewire_v_write_1( void ){

    ATOMIC;

    drive_low();
    delay_a();
    release();
    delay_b();

    END_ATOMIC;
}

void onewire_v_write_0( void ){

    ATOMIC;

    drive_low();
    delay_c();
    release();
    delay_d();

    END_ATOMIC;
}

bool onewire_b_read_bit( void ){

    ATOMIC;

    drive_low();
    delay_a();
    release();
    delay_e();
    bool bit = sample();
    delay_f();

    END_ATOMIC;

    return bit;
}

void onewire_v_write_byte( uint8_t byte ){

    for( uint8_t i = 0; i < 8; i++ ){

        if( byte & 0x01 ){

            onewire_v_write_1();
        }
        else{

            onewire_v_write_0();
        }

        byte >>= 1;
    }
}

uint8_t onewire_u8_read_byte( void ){

    uint8_t byte = 0;

    for( uint8_t i = 0; i < 8; i++ ){

        if( onewire_b_read_bit() ){

            byte |= 0x80;
        }

        byte >>= 1;
    }

    return byte;
}

#else

void onewire_v_init( void ){

}

#endif


