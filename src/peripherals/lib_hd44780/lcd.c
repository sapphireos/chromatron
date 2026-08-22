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

// Note:  This driver always operates in 4 bit mode only.
// Requires use of Adafruit LCD backpack, or similarly wired
// MCP23008 IO expander.

// in 4x40 mode, requires a GPIO pin for the second enable line.


// https://web.alfredstate.edu/faculty/weimandn/lcd/lcd_addressing/lcd_addressing_index.html
// https://www.8051projects.net/lcd-interfacing/lcd-custom-character.php

#include <stdarg.h>

#include "sapphire.h"

#include "mcp23008.h"
#include "lcd.h"

#ifdef ESP32

static uint8_t lcd_size_x;
static uint8_t lcd_size_y;


#define StrobeE() \
    mcp23008_v_digital_write( LCD_ENABLE_PIN, TRUE ); \
	mcp23008_v_digital_write( LCD_ENABLE_PIN, FALSE )

// #ifdef LCD_ENABLE_2_PRESENT
#define StrobeE2() \
    mcp23008_v_digital_write( LCD_ENABLE2_PIN, TRUE ); \
    mcp23008_v_digital_write( LCD_ENABLE2_PIN, FALSE )
// #endif


static void mcp23008_v_clear( uint8_t pin ){

    mcp23008_v_digital_write( pin, 0 );
}

static void mcp23008_v_set( uint8_t pin ){

    mcp23008_v_digital_write( pin, 1 );
}

static void Port4BitWrite( uint8_t data, uint8_t en ){

    // mcp23008_v_clear_mask( ( 1 << LCD_DATA_PIN_D4 ) |
    //                        ( 1 << LCD_DATA_PIN_D5 ) |
    //                        ( 1 << LCD_DATA_PIN_D6 ) |
    //                        ( 1 << LCD_DATA_PIN_D7 ) );

    uint8_t mask = 0;

    if( data & 0x01 ){
        mask |= ( 1 << LCD_DATA_PIN_D4 );
    }

    if( data & 0x02 ){
        mask |= ( 1 << LCD_DATA_PIN_D5 );
    }

    if( data & 0x04 ){
        mask |= ( 1 << LCD_DATA_PIN_D6 );
    }

    if( data & 0x08 ){
        mask |= ( 1 << LCD_DATA_PIN_D7 );
    }   

    // mcp23008_v_set_mask( mask );
    mcp23008_v_write_mask( 0xF0, mask );

    if( en == 0 ){

        StrobeE();
    }
    // #ifdef LCD_ENABLE_2_PRESENT
    else{

        StrobeE2();
    }
    // #endif
}

static void Port8BitWrite( uint8_t data, uint8_t en ){

    Port4BitWrite( data >> 4, en );
    // _delay_us( 10 );
	Port4BitWrite( data & 0xff, en );
    // _delay_us( 10 );
}

static bool is_enable2( void ){

    return lcd_size_x > 20;
}

void lcd_v_init( uint8_t size_x, uint8_t size_y ){

    lcd_size_x = size_x;
    lcd_size_y = size_y;

    mcp23008_v_init();

    // initialize IO
    mcp23008_v_set_mode( LCD_ENABLE_PIN, IO_MODE_OUTPUT );
    mcp23008_v_set_mode( LCD_RS_PIN, IO_MODE_OUTPUT );
    mcp23008_v_set_mode( LCD_DATA_PIN_D4, IO_MODE_OUTPUT );
    mcp23008_v_set_mode( LCD_DATA_PIN_D5, IO_MODE_OUTPUT );
    mcp23008_v_set_mode( LCD_DATA_PIN_D6, IO_MODE_OUTPUT );
    mcp23008_v_set_mode( LCD_DATA_PIN_D7, IO_MODE_OUTPUT );

    if( is_enable2() ){

        mcp23008_v_set_mode( LCD_ENABLE2_PIN, IO_MODE_OUTPUT );
    }

    // mcp23008_v_set_mode( LCD_BACKLIGHT_PIN, IO_MODE_OUTPUT );

    // #ifdef LCD_ENABLE_2_PRESENT
    // // init enable 2
    // io_v_set_mode( LCD_ENABLE_2_GPIO, IO_MODE_OUTPUT );
    // #endif


    // wait 20 ms
    _delay_ms( 20 );

    // clear RS
    mcp23008_v_clear( LCD_RS_PIN );


    // write 0x03
    Port4BitWrite( 0x03, 0 );

    _delay_ms( 5 );
    StrobeE();
    _delay_ms( 5 );
    StrobeE();
    _delay_ms( 5 );

    // write 0x02
    Port4BitWrite( 0x02, 0 );

    _delay_ms( 1 );
	Port8BitWrite( 0x28, 0 ); // function set

	_delay_ms( 1 );
	Port8BitWrite( 0x08, 0 ); // display off

	_delay_ms( 1 );
	Port8BitWrite( 0x01, 0 );

	_delay_ms( 3 );

	_delay_ms( 1 );
	Port8BitWrite( 0x06, 0 ); // entry mode

	_delay_ms( 1 );
	Port8BitWrite( 0x0C, 0 ); // display on, cursor off, blink off
	// Port8BitWrite( 0x0F, 0 ); // display on, cursor on, blink on

	_delay_ms( 1 );
	Port8BitWrite( 0x02, 0 ); // cursor home

    // #ifdef LCD_ENABLE_2_PRESENT
    if( is_enable2() ){

        // write 0x03
        Port4BitWrite( 0x03, 1 );

        _delay_ms( 5 );
        StrobeE2();
        _delay_ms( 5 );
        StrobeE2();
        _delay_ms( 5 );

        // write 0x02
        Port4BitWrite( 0x02, 1 );

        _delay_ms( 1 );
        Port8BitWrite( 0x28, 1 ); // function set

        _delay_ms( 1 );
        Port8BitWrite( 0x08, 1 ); // display off

        _delay_ms( 1 );
        Port8BitWrite( 0x01, 1 );

        _delay_ms( 3 );

        _delay_ms( 1 );
        Port8BitWrite( 0x06, 1 ); // entry mode

        _delay_ms( 1 );
        Port8BitWrite( 0x0C, 1 ); // display on, cursor off, blink off
        // Port8BitWrite( 0x0F, 1 ); // display on, cursor on, blink on

        _delay_ms( 1 );
        Port8BitWrite( 0x02, 1 ); // cursor home
        // #endif
    }

    _delay_ms( 3 );
	// initialization complete


    // lcd_v_set_backlight( TRUE );

    lcd_v_clear();
}


void lcd_v_clear( void ){

    mcp23008_v_clear( LCD_RS_PIN );

    Port8BitWrite( 0x01, 0 );

    // #`ifdef LCD_ENABLE_2_PRESENT
    if( is_enable2() ){
        
        Port8BitWrite( 0x01, 1 );
    }
    // #endif

    // this command takes 1.64ms
    _delay_ms( 2 );
}

void lcd_v_write( char *s, uint8_t x, uint8_t y ){

    ASSERT( x < lcd_size_x );
    ASSERT( y < lcd_size_y );
    
    uint8_t len = strlen( s );

    if( ( x + len ) > lcd_size_x ){

        len = lcd_size_x - x;
    }

    lcd_v_cursor( x, y );


    mcp23008_v_set( LCD_RS_PIN );

    for( uint8_t i = 0; i < len; i++ ){

        // #ifdef LCD_ENABLE_2_PRESENT
        if( is_enable2() ){

            if( y < 2 ){
                
                Port8BitWrite( s[i], 0 );
            }
            else{

                Port8BitWrite( s[i], 1 );   
            }
        // #else
        }
        else{

            Port8BitWrite( s[i], 0 );
        }
        // #endif
    }
}

void lcd_v_write_P( PGM_P s, uint8_t x, uint8_t y ){

    char buf[lcd_size_x + 1];

    strlcpy_P( buf, s, sizeof(buf) );

    lcd_v_write( buf, x, y );
}

void lcd_v_cursor( uint8_t x, uint8_t y ){

    mcp23008_v_clear( LCD_RS_PIN );

    if( y == 0 ){

        Port8BitWrite( 0x80 + 0 + x, 0 );
    }
    else if( y == 1 ){

        Port8BitWrite( 0x80 + 0x40 + x, 0 );
    }
    else if( y == 2 ){

        if( is_enable2() ){

            // Port8BitWrite( 0x80 + 0x14 + x, 1 );
            Port8BitWrite( 0x80 + 0 + x, 1 );
        }
        else{

            Port8BitWrite( 0x80 + 0x14 + x, 0 );
        }
    }
    else if( y == 3 ){

        if( is_enable2() ){

            // Port8BitWrite( 0x80 + 0x54 + x, 1 );
            Port8BitWrite( 0x80 + 0x40 + x, 1 );
        }
        else{

            Port8BitWrite( 0x80 + 0x54 + x, 0 );
        }
    }

    // #ifdef LCD_ENABLE_2_PRESENT
    // else if( y == 2 ){

    //     Port8BitWrite( 0x80 + 0 + x, 1 );
    // }
    // else if( y == 3 ){

    //     Port8BitWrite( 0x80 + 0x40 + x, 1 );
    // }
    // #endif
}

void lcd_v_printf_P( uint8_t x, uint8_t y, PGM_P format, ... ){

    char buf[128];
    uint8_t len = 0;

    va_list ap;

    // parse variable arg list
    va_start( ap, format );

    len += vsnprintf_P( buf, sizeof(buf), format, ap );

    va_end( ap );

    lcd_v_write( buf, x, y );
}

void lcd_v_set_cgram( uint8_t index, uint8_t values[8] ){

    mcp23008_v_clear( LCD_RS_PIN );

    Port8BitWrite( 0x40 + index * 8, 0 );

    if( is_enable2() ){

        Port8BitWrite( 0x40 + index * 8, 1 );
    }

    mcp23008_v_set( LCD_RS_PIN );

    Port8BitWrite( values[0], 0 );
    Port8BitWrite( values[1], 0 );
    Port8BitWrite( values[2], 0 );
    Port8BitWrite( values[3], 0 );
    Port8BitWrite( values[4], 0 );
    Port8BitWrite( values[5], 0 );
    Port8BitWrite( values[6], 0 );
    Port8BitWrite( values[7], 0 );

    if( is_enable2() ){

        Port8BitWrite( values[0], 1 );
        Port8BitWrite( values[1], 1 );
        Port8BitWrite( values[2], 1 );
        Port8BitWrite( values[3], 1 );
        Port8BitWrite( values[4], 1 );
        Port8BitWrite( values[5], 1 );
        Port8BitWrite( values[6], 1 );
        Port8BitWrite( values[7], 1 );
    }
}

void lcd_v_write_char( uint8_t c, uint8_t x, uint8_t y ){

    lcd_v_cursor( x, y );

    mcp23008_v_set( LCD_RS_PIN );

    if( is_enable2() ){

        if( y < 2 ){
            
            Port8BitWrite( c, 0 );
        }
        else{

            Port8BitWrite( c, 1 );   
        }
    // #else
    }
    else{

        Port8BitWrite( c, 0 );
    }
    // #endif
}

#endif