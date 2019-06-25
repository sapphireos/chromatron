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

#include "hal_io.h"

#include "hal_i2c.h"

#include "i2c.h"




/*

This is a bit bang driver, so it can work on any set of pins.

*/

static uint8_t delay_1;
static uint8_t delay_2;


// map delays to KV system for easy hand tuning
// #include "keyvalue.h"
// KV_SECTION_META kv_meta_t meow_info_kv[] = {
//     { SAPPHIRE_TYPE_UINT8,  0, 0, &delay_1,                   0,   "delay_1" },
//     { SAPPHIRE_TYPE_UINT8,  0, 0, &delay_2,                   0,   "delay_2" }, 
// };



static io_port_t *scl_port = &IO_PIN0_PORT;
static uint8_t scl_pin = ( 1 << IO_PIN0_PIN );
static io_port_t *sda_port = &IO_PIN1_PORT;
static uint8_t sda_pin = ( 1 << IO_PIN1_PIN );


static void wait_while_clock_stretch( void ){

    volatile uint8_t c;
    volatile uint32_t timeout = 10000;    

    do{
        c = scl_port->IN & scl_pin;

        timeout--;

    } while( ( c == 0 ) && ( timeout > 0 ) );
}


#define SDA_HIGH() ( sda_port->OUTSET = sda_pin )
#define SDA_LOW() ( sda_port->OUTCLR = sda_pin )

#define SCL_HIGH() ( scl_port->OUTSET = scl_pin )
#define SCL_LOW() ( scl_port->OUTCLR = scl_pin )


#define I2C_DELAY() for( uint8_t __i = 0; __i < 8; __i++ ){ asm volatile("nop"); }

#define I2C_DELAY_1() for( uint8_t __i = delay_1; __i > 0; __i-- ){ asm volatile("nop"); }
#define I2C_DELAY_2() for( uint8_t __i = delay_2; __i > 0; __i-- ){ asm volatile("nop"); }


static void send_bit( uint8_t b ){

    if( b ){

        SDA_HIGH();
    }
    else{

        SDA_LOW();
    }

    I2C_DELAY_1();

    SCL_HIGH();
    wait_while_clock_stretch();

    I2C_DELAY_2();
    
    SCL_LOW();
}


static bool read_bit( void ){

    // release bus
    SDA_HIGH();

    I2C_DELAY_1();

    SCL_HIGH();
    wait_while_clock_stretch();

    I2C_DELAY_2();
    
    // read bit
    uint8_t b = sda_port->IN & sda_pin;

    SCL_LOW();

    return b != 0;
}

static uint8_t i2c_v_send_byte( uint8_t b ){
    
    send_bit( b & 0x80 );
    send_bit( b & 0x40 );
    send_bit( b & 0x20 );
    send_bit( b & 0x10 );
    send_bit( b & 0x08 );
    send_bit( b & 0x04 );
    send_bit( b & 0x02 );
    send_bit( b & 0x01 );

    // ack
    return read_bit();
}


static uint8_t i2c_u8_read_byte( bool ack ){

    uint8_t b = 0;

    if( read_bit() ){

        b |= 0x80;
    }
    if( read_bit() ){

        b |= 0x40;
    }
    if( read_bit() ){

        b |= 0x20;
    }
    if( read_bit() ){

        b |= 0x10;
    }
    if( read_bit() ){

        b |= 0x08;
    }
    if( read_bit() ){

        b |= 0x04;
    }
    if( read_bit() ){

        b |= 0x02;
    }
    if( read_bit() ){

        b |= 0x01;
    }

    if( ack ){

        send_bit( 0 );
    }
    else{

        send_bit( 1 );
    }

    return b;
}

void i2c_v_init( i2c_baud_t8 baud ){

    // these are all hand timed.
    if( baud == I2C_BAUD_400K ){

        delay_1 = 0;
        delay_2 = 5;
    }
    else if( baud == I2C_BAUD_300K ){

        delay_1 = 1;
        delay_2 = 9;   
    }
    else if( baud == I2C_BAUD_200K ){

        delay_1 = 4;
        delay_2 = 15;   
    }
    else{

        // default is 100K

        delay_1 = 18;
        delay_2 = 28;   
    }

    i2c_v_set_pins( IO_PIN_1_XCK, IO_PIN_0_GPIO );
}

void i2c_v_set_pins( uint8_t clock, uint8_t data ){

    // set old pins to input
    io_v_set_mode( clock, IO_MODE_INPUT );
    io_v_set_mode( data, IO_MODE_INPUT );

    scl_port = io_p_get_port( clock );
    scl_pin = ( 1 << io_u8_get_pin( clock ) );
    sda_port = io_p_get_port( data );
    sda_pin = ( 1 << io_u8_get_pin( data ) );
    
    io_v_set_mode( clock, IO_MODE_OUTPUT_OPEN_DRAIN );
    io_v_set_mode( data, IO_MODE_OUTPUT_OPEN_DRAIN );

    SDA_HIGH();
    SCL_HIGH();

    I2C_DELAY();
}

uint8_t i2c_u8_status( void ){

    return 0;
}

static void i2c_v_start( void ){

    SDA_HIGH();
    SCL_HIGH();
    wait_while_clock_stretch();
    I2C_DELAY();

    SDA_LOW();
    I2C_DELAY();

    SCL_LOW();
    I2C_DELAY();
}

static void i2c_v_stop( void ){

    I2C_DELAY();

    SDA_LOW();
    I2C_DELAY();

    SCL_HIGH();
    wait_while_clock_stretch();
    I2C_DELAY();

    SDA_HIGH();
    I2C_DELAY();
}

static void i2c_v_send_address( uint8_t dev_addr, bool write ){

    if( write ){
        
        i2c_v_send_byte( ( dev_addr << 1 ) & ~0x01 );
    }
    else{

        i2c_v_send_byte( ( dev_addr << 1 ) | 0x01 );   
    }
}

void i2c_v_write( uint8_t dev_addr, const uint8_t *src, uint8_t len ){

    i2c_v_start();

    i2c_v_send_address( dev_addr, TRUE );

    while( len > 0 ){

        i2c_v_send_byte( *src );

        src++;
        len--;
    }

    i2c_v_stop();
}

void i2c_v_read( uint8_t dev_addr, uint8_t *dst, uint8_t len ){

    i2c_v_start();

    i2c_v_send_address( dev_addr, FALSE );

    while( len > 0 ){

        if( len > 1 ){

            *dst = i2c_u8_read_byte( TRUE );
        }
        else{

            *dst = i2c_u8_read_byte( FALSE );
        }

        dst++;
        len--;
    }

    i2c_v_stop();
}

void i2c_v_mem_write( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, const uint8_t *src, uint8_t len, uint16_t delay_ms ){

    i2c_v_start();

    i2c_v_send_address( dev_addr, TRUE );

    if( addr_size == 1 ){
        
        i2c_v_send_byte( mem_addr );
    }
    else if( addr_size == 2 ){
        
        i2c_v_send_byte( mem_addr >> 8 );        
        i2c_v_send_byte( mem_addr & 0xff );        
    }
    else{

        ASSERT( FALSE );
    }

    _delay_ms( delay_ms );

    i2c_v_write( dev_addr, src, len );
}

void i2c_v_mem_read( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, uint8_t *dst, uint8_t len, uint16_t delay_ms ){
    
    i2c_v_start();

    i2c_v_send_address( dev_addr, TRUE );

    if( addr_size == 1 ){
        
        i2c_v_send_byte( mem_addr );
    }
    else if( addr_size == 2 ){
        
        i2c_v_send_byte( mem_addr >> 8 );        
        i2c_v_send_byte( mem_addr & 0xff );        
    }
    else{

        ASSERT( FALSE );
    }

    _delay_ms( delay_ms );

    i2c_v_read( dev_addr, dst, len );   
}


