// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "sapphire.h"

#include "veml7700.h"

#ifndef ESP8266

/*
Luminance Example                                                                  
10-5 lx Light from Sirius, the brightest star in the night sky
10-4 lx Total starlight, overcast sky
0.002 lx Moonless clear night sky with airglow
0.01 lx Quarter moon, 0.27 lx; full moon on a clear night

1 lx Full moon overhead at tropical latitudes
3.4 lx  Dark limit of civil twilight under a clear sky
50 lx Family living room

80 lx Hallway / bathroom

100 lx Very dark overcast day
320 lx to 500 lx Office lighting

400 lx Sunrise or sunset on a clear day
1000 lx Overcast day; typical TV studio lighting
10 000 lx to 25 000 lx Full daylight (not direct sun)
32 000 lx to 130 000 lx  Direct sunlight
*/

// values in lux
static uint32_t als;
static uint32_t white;
static uint16_t raw_als;
static uint16_t raw_white;

static uint8_t gain;
static uint8_t int_time;

KV_SECTION_META kv_meta_t veml7700_kv[] = {
    {CATBUS_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &als,         0, "veml7700_als"},   
    {CATBUS_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &white,       0, "veml7700_white"},   
    {CATBUS_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY, &raw_als,     0, "veml7700_raw_als"},   
    {CATBUS_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY, &raw_white,   0, "veml7700_raw_white"},   

    {CATBUS_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &gain,        0, "veml7700_gain"},   
    {CATBUS_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &int_time,    0, "veml7700_int_time"},   
};


static void _write_reg16( uint8_t reg, uint16_t val ){

    i2c_v_mem_write( VEML7700_I2C_ADDR, reg, 1, (uint8_t *)&val, sizeof(val), 5 );
}

static uint16_t _read_reg16( uint8_t reg ){

    uint16_t val = 0;
    i2c_v_mem_read( VEML7700_I2C_ADDR, reg, 1, (uint8_t *)&val, sizeof(val), 5 );

    return val;
}	

static void set_shutdown( bool shutdown ){

    uint16_t reg = _read_reg16( VEML7700_REG_ALS_CONF_0 );

    if( shutdown ){

        reg |= VEML7700_BIT_ALS_SD;
    }
    else{

        reg &= ~VEML7700_BIT_ALS_SD;
    }

    _write_reg16( VEML7700_REG_ALS_CONF_0, reg );
}

void veml7700_v_configure( uint8_t _gain, uint8_t _int_time ){

    set_shutdown( TRUE );

    gain = _gain;
    int_time = _int_time;

    uint16_t config = _read_reg16( VEML7700_REG_ALS_CONF_0 );
    config &= ~( VEML7700_ALS_GAIN_MASK << VEML7700_ALS_GAIN_SHIFT );
    config |= ( ( gain & VEML7700_ALS_GAIN_MASK ) << VEML7700_ALS_GAIN_SHIFT );

    config &= ~( VEML7700_ALS_INT_TIME_MASK << VEML7700_ALS_INT_TIME_SHIFT );
    config |= ( ( int_time & VEML7700_ALS_INT_TIME_MASK ) << VEML7700_ALS_INT_TIME_SHIFT );

    _write_reg16( VEML7700_REG_ALS_CONF_0, config );

    set_shutdown( FALSE );
}

uint16_t veml7700_u16_read_als( void ){

    return _read_reg16( VEML7700_REG_ALS );
}

uint16_t veml7700_u16_read_white( void ){

    return _read_reg16( VEML7700_REG_WHITE );
}

static uint32_t calc_lux( uint16_t val, uint8_t _gain, uint8_t _int_time ){

    // we will calculate in microlux to maximize resolution without
    // using floating point.

    // this function will then convert down to lux before returning.

    // resolution set for gain with 100 ms integration time
    // units are microlux
    // note the use of 64 bits to have enough range!
    uint64_t resolution = 0;

    if( _gain == VEML7700_ALS_GAIN_x1 ){

        resolution = 57600;
    }
    else if( _gain == VEML7700_ALS_GAIN_x2 ){

        resolution = 28800;
    }
    else if( _gain == VEML7700_ALS_GAIN_x0_125 ){

        resolution = 460800;
    }
    else if( _gain == VEML7700_ALS_GAIN_x0_25 ){

        resolution = 230400;
    }
    else{

        log_v_error_P( PSTR("invalid gain") );

        // invalid setting
        return 0;
    }

    if( _int_time == VEML7700_ALS_INT_TIME_25ms ){

        resolution *= 4;
    }
    else if( _int_time == VEML7700_ALS_INT_TIME_50ms ){

        resolution *= 2;
    }
    else if( _int_time == VEML7700_ALS_INT_TIME_100ms ){

        resolution *= 1;        
    }
    else if( _int_time == VEML7700_ALS_INT_TIME_200ms ){

        resolution /= 2;        
    }
    else if( _int_time == VEML7700_ALS_INT_TIME_400ms ){

        resolution /= 4;
    }
    else if( _int_time == VEML7700_ALS_INT_TIME_800ms ){

        resolution /= 8;
    }
    else{

        log_v_error_P( PSTR("invalid int time") );

        // invalid setting
        return 0;
    }

    // return lux
    // return ( (uint64_t)val * resolution ) / 1000000;

    // return millilux
    return ( (uint64_t)val * resolution ) / 1000;
}


PT_THREAD( veml7700_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );
    
    // detect chip: 
    uint16_t config = ( VEML7700_ALS_GAIN_x0_25 << VEML7700_ALS_GAIN_SHIFT );
    ASSERT( config != 0 );
    
    _write_reg16( VEML7700_REG_ALS_CONF_0, config );

    if( _read_reg16( VEML7700_REG_ALS_CONF_0 ) != config ){

        THREAD_EXIT( pt );
    }

    // disable PSM mode
    // this sets highest sample rate
    _write_reg16( VEML7700_REG_PSM, 0 );

    log_v_info_P( PSTR("VEML7700 detected") );

    
    // veml7700_v_configure( VEML7700_ALS_GAIN_x0_125, VEML7700_ALS_INT_TIME_25ms );
    // veml7700_v_configure( VEML7700_ALS_GAIN_x2, VEML7700_ALS_INT_TIME_800ms );

    while(1){

        TMR_WAIT( pt, 1000 );

        raw_als = veml7700_u16_read_als();
        raw_white = veml7700_u16_read_white();

        if( ( raw_als < 100 ) &&
            ( gain != VEML7700_ALS_GAIN_x2 ) ){

            log_v_debug_P( PSTR("VEML7700 switching to high gain") );

            veml7700_v_configure( VEML7700_ALS_GAIN_x2, VEML7700_ALS_INT_TIME_800ms );
            continue;
        }
        else if( ( raw_als > 20000 ) &&
            ( gain != VEML7700_ALS_GAIN_x0_125 ) ){

            log_v_debug_P( PSTR("VEML7700 switching to low gain") );

            veml7700_v_configure( VEML7700_ALS_GAIN_x0_125, VEML7700_ALS_INT_TIME_25ms );
            continue;
        }

        als = calc_lux( raw_als, gain, int_time );
        white = calc_lux( raw_white, gain, int_time );

        // log_v_debug_P( PSTR("%d %d"), raw_als, raw_white );
    }
    

PT_END( pt );	
}


void veml7700_v_init( void ){
    
    i2c_v_init( I2C_BAUD_400K );

    thread_t_create( veml7700_thread,
                     PSTR("veml7700"),
                     0,
                     0 );
}

#endif