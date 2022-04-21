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

#include "sapphire.h"

#include "ads1015.h"

// Notes:
// This driver only supports single ended mode

int8_t ads1015_i8_init( uint8_t device_addr ){

    if( device_addr == 0 ){

        device_addr = ADS1015_I2C_ADDR0;
    }

    // detect chip

    uint8_t cmd[3];
    cmd[0] = ADS1015_REG_LO_THRESH;
    cmd[1] = 0;
    cmd[2] = 0;

    i2c_v_write( device_addr, cmd, sizeof(cmd) );

    cmd[1] = 0x12;    
    cmd[2] = 0x30;    
    i2c_v_write( device_addr, cmd, sizeof(cmd) );


    // read back
    i2c_v_write( device_addr, &cmd[0], sizeof(cmd[0]) );    

    uint8_t temp[2] = {0};
    i2c_v_read( device_addr, temp, sizeof(temp) );

    if( ( temp[0] != 0x12 ) && 
        ( temp[1] != 0x30 ) ){

        log_v_debug_P( PSTR("%x %x ???"), temp[0], temp[1] );

        return -1;
    }

    cmd[1] = 0;
    cmd[2] = 0;
    i2c_v_write( device_addr, cmd, sizeof(cmd) );

    return 0;
}

 
void ads1015_v_start( uint8_t device_addr, uint8_t channel, uint8_t gain_setting ){

    if( device_addr == 0 ){

        device_addr = ADS1015_I2C_ADDR0;
    }

    uint16_t rate = ADS1015_RATE_3300;

    uint16_t gain_config = 0;
    if( gain_setting == ADS1015_GAIN_256 ){

        gain_config = ADS1015_CFG_GAIN_256;
    }
    else if( gain_setting == ADS1015_GAIN_512 ){

        gain_config = ADS1015_CFG_GAIN_512;
    }
    else if( gain_setting == ADS1015_GAIN_1024 ){

        gain_config = ADS1015_CFG_GAIN_1024;
    }
    else if( gain_setting == ADS1015_GAIN_2048 ){

        gain_config = ADS1015_CFG_GAIN_2048;
    }
    else if( gain_setting == ADS1015_GAIN_4096 ){

        gain_config = ADS1015_CFG_GAIN_4096;
    }

    // write config
    uint16_t config = ADS1015_OS_CONV_START | gain_config | ADS1015_MODE_SINGLE_SHOT | rate;

    switch( channel ){
        case 0:
            config |= ADS1015_MUX_AIN0;
            break;

        case 1:
            config |= ADS1015_MUX_AIN1;
            break;
        
        case 2:
            config |= ADS1015_MUX_AIN2;
            break;
    
        case 3:
            config |= ADS1015_MUX_AIN3;
            break;
        
        default:
            break;       
    }

    // start conversion
    uint8_t cmd[3];
    cmd[0] = ADS1015_REG_CFG;
    cmd[1] = config >> 8;
    cmd[2] = config & 0xff;

    i2c_v_write( device_addr, cmd, sizeof(cmd) );

    _delay_us( 30 ); // ensure conversion has started before we can poll
}

bool ads1015_b_poll( uint8_t device_addr, uint8_t channel, uint8_t gain_setting, int32_t *microvolts ){

    if( device_addr == 0 ){

        device_addr = ADS1015_I2C_ADDR0;
    }

    uint8_t reg = ADS1015_REG_CFG;
    i2c_v_write( device_addr, &reg, sizeof(reg) );
    
    uint16_t config_reg = 0;
    i2c_v_read( device_addr, (uint8_t*)&config_reg, sizeof(config_reg) );

    if( ( config_reg & ADS1015_CONV_DONE ) == 0 ){

        return FALSE;
    }

    reg = ADS1015_REG_CONV;
    i2c_v_write( device_addr, &reg, sizeof(reg) );

    uint8_t temp[2] = {0};
    i2c_v_read( device_addr, temp, sizeof(temp) );

    int32_t result = ( ( (uint16_t)temp[0] << 8 ) | temp[1] ) >> 4;

    if( result >= 0x800 ){

        result -= 0x1000; // invert sign
    }

    // convert to microvolts
    if( gain_setting == ADS1015_GAIN_256 ){

        result *= 125;    
    }
    else if( gain_setting == ADS1015_GAIN_512 ){

        result *= 250;    
    }
    else if( gain_setting == ADS1015_GAIN_1024 ){

        result *= 500;    
    }
    else if( gain_setting == ADS1015_GAIN_2048 ){

        result *= 1000;    
    }
    else if( gain_setting == ADS1015_GAIN_4096 ){

        result *= 2000;    
    }

    *microvolts = result;

    return TRUE;
}

// returns microvolts 
int32_t ads1015_i32_read( uint8_t device_addr, uint8_t channel, uint8_t gain_setting ){

    if( device_addr == 0 ){

        device_addr = ADS1015_I2C_ADDR0;
    }

    ads1015_v_start( device_addr, channel, gain_setting );

    uint32_t timeout = 100;

    while( timeout > 0 ){

        int32_t result = 0;

        if( ads1015_b_poll( device_addr, channel, gain_setting, &result ) ){

            return result;
        }

        timeout--;
    }

    return 0;
}

int32_t ads1015_i32_read_filtered( uint8_t device_addr, uint8_t channel, uint8_t samples, uint8_t gain_setting ){

    if( device_addr == 0 ){

        device_addr = ADS1015_I2C_ADDR0;
    }

    if( samples == 0){

        return 0;
    }

    int32_t accumulator = 0;

    for( uint8_t i = 0; i < samples; i++ ){

        accumulator += ads1015_i32_read( device_addr, channel, gain_setting );
    }

    return accumulator / samples;
}
