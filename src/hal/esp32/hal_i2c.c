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

#include "system.h"

#include "hal_io.h"
#include "hal_i2c.h"

#include "i2c.h"
#include "driver/i2c.h"

// feather defaults
static gpio_num_t gpio_sda = GPIO_NUM_23;
static gpio_num_t gpio_scl = GPIO_NUM_22;

static i2c_baud_t8 i2c_baud;

// static i2c_cmd_handle_t handle;

#define CHECK_ACK   TRUE
#define NO_ACK      FALSE

void i2c_v_init( i2c_baud_t8 baud ){

    // if( handle == 0 ){

    //     handle = i2c_cmd_link_create();    
    // }
    
    i2c_baud = baud;

    i2c_config_t conf = {0};

    conf.mode               = I2C_MODE_MASTER;
    conf.sda_io_num         = gpio_sda;
    conf.sda_pullup_en      = GPIO_PULLUP_ENABLE;
    conf.scl_io_num         = gpio_scl;
    conf.scl_pullup_en      = GPIO_PULLUP_ENABLE;

    if( i2c_baud == I2C_BAUD_100K ){

        conf.master.clk_speed = 100000;        
    }
    else if( i2c_baud == I2C_BAUD_200K ){

        conf.master.clk_speed = 200000;        
    }
    else if( i2c_baud == I2C_BAUD_300K ){

        conf.master.clk_speed = 300000;        
    }
    else if( i2c_baud == I2C_BAUD_400K ){

        conf.master.clk_speed = 400000;        
    }
    else if( i2c_baud == I2C_BAUD_1000K ){

        conf.master.clk_speed = 1000000;        
    }

    gpio_set_direction( gpio_sda, GPIO_MODE_OUTPUT_OD );
    gpio_set_direction( gpio_scl, GPIO_MODE_OUTPUT_OD );

    i2c_param_config( I2C_MASTER_PORT, &conf );

    i2c_driver_install( I2C_MASTER_PORT, conf.mode, 0, 0, 0 );

    // i2c_set_timeout( I2C_MASTER_PORT, 50 / portTICK_RATE_MS );
}

void i2c_v_set_pins( uint8_t clock, uint8_t data ){
	
    gpio_sda = hal_io_i32_get_gpio_num( data );
    gpio_scl = hal_io_i32_get_gpio_num( clock );
}

void i2c_v_write( uint8_t dev_addr, const uint8_t *src, uint8_t len ){

    i2c_master_write_to_device( I2C_MASTER_PORT, dev_addr, (uint8_t *)src, len, 50 / portTICK_RATE_MS );

    // i2c_master_start( handle );

    // // send address
    // i2c_master_write_byte( handle, ( dev_addr << 1 ) | I2C_MASTER_WRITE, CHECK_ACK );
    
    // // send data
    // i2c_master_write( handle, (uint8_t *)src, len, CHECK_ACK );

    // i2c_master_stop( handle );

    // // run the command sequence
    // i2c_master_cmd_begin( I2C_MASTER_PORT, handle, 50 / portTICK_RATE_MS );
}

void i2c_v_read( uint8_t dev_addr, uint8_t *dst, uint8_t len ){

    i2c_master_read_from_device( I2C_MASTER_PORT, dev_addr, dst, len, 50 / portTICK_RATE_MS );

    // i2c_master_start( handle );

    // // send address
    // i2c_master_write_byte( handle, ( dev_addr << 1 ) | I2C_MASTER_READ, CHECK_ACK );
    
    // // read data
    // i2c_master_read( handle, dst, len, I2C_MASTER_ACK );

    // i2c_master_stop( handle );

    // // run the command sequence
    // i2c_master_cmd_begin( I2C_MASTER_PORT, handle, 50 / portTICK_RATE_MS );
}

void i2c_v_mem_write( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, const uint8_t *src, uint8_t len, uint16_t delay_ms ){

    uint8_t buf[255];

    ASSERT( len + addr_size < sizeof(buf) );

    memcpy( buf, &mem_addr, addr_size );
    memcpy( &buf[addr_size], src, len );

    i2c_master_write_to_device( I2C_MASTER_PORT, dev_addr, buf, len + addr_size, 50 / portTICK_RATE_MS );

    // i2c_master_start( handle );

    // // send address
    // i2c_master_write_byte( handle, ( dev_addr << 1 ) | I2C_MASTER_WRITE, CHECK_ACK );
    
    // // send reg addr
    // i2c_master_write( handle, (uint8_t *)&mem_addr, addr_size, CHECK_ACK );

    // // send data
    // i2c_master_write( handle, (uint8_t *)src, len, CHECK_ACK );

    // i2c_master_stop( handle );

    // // run the command sequence
    // i2c_master_cmd_begin( I2C_MASTER_PORT, handle, 50 / portTICK_RATE_MS );
}

void i2c_v_mem_read( uint8_t dev_addr, uint16_t mem_addr, uint8_t addr_size, uint8_t *dst, uint8_t len, uint16_t delay_ms ){

    i2c_master_write_read_device( I2C_MASTER_PORT, dev_addr, (uint8_t *)&mem_addr, addr_size, dst, len, 50 / portTICK_RATE_MS );

    // i2c_master_start( handle );

    // // send address
    // i2c_master_write_byte( handle, ( dev_addr << 1 ) | I2C_MASTER_WRITE, CHECK_ACK );
    
    // // send reg addr
    // i2c_master_write( handle, (uint8_t *)&mem_addr, addr_size, CHECK_ACK );

    // i2c_master_start( handle );

    // // send address
    // i2c_master_write_byte( handle, ( dev_addr << 1 ) | I2C_MASTER_READ, CHECK_ACK );

    // // read data
    // i2c_master_read( handle, dst, len, I2C_MASTER_ACK );

    // i2c_master_stop( handle );

    // // run the command sequence
    // i2c_master_cmd_begin( I2C_MASTER_PORT, handle, 50 / portTICK_RATE_MS );
}

